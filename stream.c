/* SPDX-License-Identifier: Apache-2.0 */

#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <cobs.h>

static size_t net_buf_pull_across_fragments(struct net_buf **const pbuf, void *const output_,
					    size_t length)
{
	uint8_t *output = output_;
	size_t num_read = 0;

	while (length && *pbuf) {
		const size_t pull_length = MIN((*pbuf)->len, length);

		if (pull_length > 0) {
			const void *data = net_buf_pull_mem(*pbuf, pull_length);
			memcpy(output, data, pull_length);

			output += pull_length;
			length -= pull_length;
			num_read += pull_length;
		}

		if ((*pbuf)->len == 0) {
			struct net_buf *const empty_frag = *pbuf;

			*pbuf = empty_frag->frags;
			empty_frag->frags = NULL;

			net_buf_unref(empty_frag);
		}
	}

	return num_read;
}

static int net_buf_find_zero(struct cobs_encode *encode, size_t *num_processed,
			     size_t *zero_position)
{
	size_t offset = 0;

	struct net_buf *buf = encode->buf;
	while (buf) {
		for (uint16_t i = 0; i < buf->len; i += 1, offset += 1) {
			if (buf->data[i] == 0) {
				*num_processed = offset + 1;
				*zero_position = offset;
				return 0;
			}
		}

		buf = buf->frags;
	}

	*num_processed = offset;
	return -ENOENT;
}

enum cobs_decode_result cobs_decode_stream(struct cobs_decode *decode, uint8_t input_byte,
					   uint8_t *output_byte, bool *output_available)
{
	*output_available = false;

	switch (decode->state) {
	case COBS_DECODE_STATE_CODE:
		if (input_byte == 0) {
			decode->state = COBS_DECODE_STATE_FINISHED;
			return COBS_DECODE_RESULT_FINISHED;
		}

		if (decode->pending_zero) {
			*output_byte = 0x00;
			*output_available = true;

			decode->pending_zero = false;
		}

		if (input_byte == 1) {
			decode->pending_zero = true;
		} else {
			decode->code = input_byte - 1;
			decode->state = COBS_DECODE_STATE_DATA;
			decode->pending_zero = input_byte != 0xFF;
		}
		return COBS_DECODE_RESULT_CONSUMED;

	case COBS_DECODE_STATE_DATA:
		if (input_byte == 0) {
			decode->state = COBS_DECODE_STATE_FINISHED;
			return COBS_DECODE_RESULT_UNEXPECTED_ZERO;
		}

		*output_byte = input_byte;
		*output_available = true;
		decode->code -= 1;

		if (decode->code == 0) {
			decode->state = COBS_DECODE_STATE_CODE;
			return COBS_DECODE_RESULT_CONSUMED;
		}
		return COBS_DECODE_RESULT_CONSUMED;

	case COBS_DECODE_STATE_FINISHED:
	default:
		return COBS_DECODE_RESULT_ERROR;
	}
}

void cobs_encode_stream_init(struct cobs_encode *encode, struct net_buf *buf)
{
	*encode = (struct cobs_encode){
		.buf = buf,
	};

	size_t num_processed;
	size_t zero_position;
	int ret = net_buf_find_zero(encode, &num_processed, &zero_position);
	if (ret == 0) {
		__ASSERT_NO_MSG(num_processed != 0);

		encode->state = zero_position ? COBS_ENCODE_STATE_ZEROS_CODE
					      : COBS_ENCODE_STATE_ZEROS_FIRSTBYTE;
		encode->u.zeros = (struct cobs_encode_zeros){
			.next_zero = zero_position,
		};
	} else {
		encode->state = COBS_ENCODE_STATE_NOZEROS_CODE;
		encode->u.nozeros = (struct cobs_encode_nozeros){
			.total_length = num_processed,
		};
	}
}

void cobs_encode_stream_free(struct cobs_encode *encode)
{
	if (encode->buf) {
		net_buf_unref(encode->buf);
	}

	*encode = (struct cobs_encode){};
}

static inline bool cobs_encode_stream_single(struct cobs_encode *encode, uint8_t *output)
{
	switch (encode->state) {
	case COBS_ENCODE_STATE_ZEROS_FIRSTBYTE:
		*output = 0x01;
		encode->state = COBS_ENCODE_STATE_ZEROS_CODE;
		break;
	case COBS_ENCODE_STATE_ZEROS_CODE: {
		if (encode->u.zeros.next_zero == 0) {
			uint8_t zero;
			size_t num_pulled = net_buf_pull_across_fragments(&encode->buf, &zero, 1);
			__ASSERT_NO_MSG(num_pulled == 1);
			__ASSERT_NO_MSG(zero == 0);

			size_t num_processed;
			size_t zero_position;
			int ret = net_buf_find_zero(encode, &num_processed, &zero_position);
			if (num_processed == 0) {
				*output = 0x01;
				encode->state = COBS_ENCODE_STATE_FINAL_ZERO;
			} else if (ret == 0) {
				__ASSERT_NO_MSG(num_processed != 0);

				encode->u.zeros.next_zero = zero_position;
				if (encode->u.zeros.next_zero == 0) {
					*output = 0x01;
				} else if (encode->u.zeros.next_zero < 254) {
					*output = encode->u.zeros.next_zero + 1;

					encode->state = COBS_ENCODE_STATE_ZEROS_DATA;
					encode->u.zeros.data_left = encode->u.zeros.next_zero;
					encode->u.zeros.post_data_state =
						COBS_ENCODE_STATE_ZEROS_CODE;
				} else {
					*output = 0xFF;

					encode->state = COBS_ENCODE_STATE_ZEROS_DATA;
					encode->u.zeros.data_left = 254;
					if (encode->u.zeros.next_zero == 254) {
						encode->u.zeros.post_data_state =
							COBS_ENCODE_STATE_ZEROS_FIRSTBYTE;
					} else {
						encode->u.zeros.post_data_state =
							COBS_ENCODE_STATE_ZEROS_CODE;
					}
				}
			} else {
				encode->state = COBS_ENCODE_STATE_NOZEROS_DATA;
				encode->u.nozeros = (struct cobs_encode_nozeros){
					.total_length = num_processed,
				};

				const size_t total_length = num_processed;
				if (total_length < 254) {
					*output = encode->u.nozeros.total_length + 1;

					encode->u.nozeros.data_left =
						encode->u.nozeros.total_length;
				} else {
					*output = 0xFF;

					encode->u.nozeros.data_left = 254;
				}
			}
		} else if (encode->u.zeros.next_zero < 254) {
			*output = encode->u.zeros.next_zero + 1;

			encode->state = COBS_ENCODE_STATE_ZEROS_DATA;
			encode->u.zeros.data_left = encode->u.zeros.next_zero;
			encode->u.zeros.post_data_state = COBS_ENCODE_STATE_ZEROS_CODE;
		} else {
			*output = 0xFF;

			encode->state = COBS_ENCODE_STATE_ZEROS_DATA;
			encode->u.zeros.data_left = 254;
			if (encode->u.zeros.next_zero == 254) {
				encode->u.zeros.post_data_state = COBS_ENCODE_STATE_ZEROS_FIRSTBYTE;
			} else {
				encode->u.zeros.post_data_state = COBS_ENCODE_STATE_ZEROS_CODE;
			}
		}

		break;
	}

	case COBS_ENCODE_STATE_ZEROS_DATA: {
		size_t num_pulled = net_buf_pull_across_fragments(&encode->buf, output, 1);
		__ASSERT_NO_MSG(num_pulled == 1);

		encode->u.zeros.data_left -= 1;
		encode->u.zeros.next_zero -= 1;

		if (encode->u.nozeros.data_left == 0) {
			encode->state = encode->u.zeros.post_data_state;
		}
		break;
	}

	case COBS_ENCODE_STATE_NOZEROS_CODE: {
		if (encode->u.nozeros.total_length == 0) {
			*output = 0x01;

			encode->state = COBS_ENCODE_STATE_FINAL_ZERO;
		} else if (encode->u.nozeros.total_length < 254) {
			*output = encode->u.nozeros.total_length + 1;

			encode->u.nozeros.data_left = encode->u.nozeros.total_length;
			encode->state = COBS_ENCODE_STATE_NOZEROS_DATA;
		} else {
			*output = 0xFF;

			encode->u.nozeros.data_left = 254;
			encode->state = COBS_ENCODE_STATE_NOZEROS_DATA;
		}
		break;
	}

	case COBS_ENCODE_STATE_NOZEROS_DATA: {
		size_t num_pulled = net_buf_pull_across_fragments(&encode->buf, output, 1);
		__ASSERT_NO_MSG(num_pulled == 1);

		encode->u.nozeros.data_left -= 1;
		encode->u.nozeros.total_length -= 1;

		if (encode->u.nozeros.total_length == 0) {
			__ASSERT_NO_MSG(encode->u.nozeros.data_left == 0);
			encode->state = COBS_ENCODE_STATE_FINAL_ZERO;
		} else if (encode->u.nozeros.data_left == 0) {
			encode->state = COBS_ENCODE_STATE_NOZEROS_CODE;
		}
		break;
	}

	case COBS_ENCODE_STATE_FINAL_ZERO:
		*output = 0x00;
		encode->state = COBS_ENCODE_STATE_FINISHED;
		break;

	case COBS_ENCODE_STATE_FINISHED:
		return true;
	}

	return false;
}

size_t cobs_encode_stream(struct cobs_encode *encode, uint8_t *output, size_t output_length)
{
	size_t i;

	for (i = 0; i < output_length; i += 1) {
		const bool done = cobs_encode_stream_single(encode, &output[i]);
		if (done) {
			return i;
		}
	}

	return i;
}
