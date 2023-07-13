/* SPDX-License-Identifier: Apache-2.0 */

#include <cobs/testutils.h>

#include <zephyr/net/buf.h>
#include <zephyr/sys/__assert.h>

NET_BUF_POOL_FIXED_DEFINE(pool, 1, 4096, 0, NULL);

void unsafe_cobs_reset_encode_pool(void)
{
	/* Incredibly unsafe and hacky.
	 * To prevent a test from failing because a previous one leaked the
	 * net_buf, we simply get a pointer to our only net_buf and unref it if
	 * it's still referenced.
	 */
	struct net_buf *const buf = (struct net_buf *)&_net_buf_pool[0].b[0];

	if (buf->ref) {
		net_buf_unref(buf);
	}
}

size_t cobs_encode_stream_simple(const uint8_t *const input, const size_t input_length,
				 uint8_t *const output, const size_t output_length)
{
	struct net_buf *const netbuf = net_buf_alloc_len(&pool, input_length, K_NO_WAIT);
	__ASSERT_NO_MSG(netbuf);
	net_buf_add_mem(netbuf, input, input_length);

	struct cobs_encode encode;
	cobs_encode_stream_init(&encode, netbuf);
	const size_t num_written = cobs_encode_stream(&encode, output, output_length);
	__ASSERT_NO_MSG(num_written <= output_length);

	const uint8_t last_byte = output[num_written - 1];
	__ASSERT(last_byte == 0x00, "last byte is 0x%02x instead ox 0x00", last_byte);

	uint8_t scratch[1];
	size_t nbytes = cobs_encode_stream(&encode, scratch, sizeof(scratch));
	__ASSERT(nbytes == 0, "There's %zu bytes of unprocessed data", nbytes);
	cobs_encode_stream_free(&encode);

	return num_written;
}

int cobs_decode_stream_simple(const uint8_t *const input, const size_t input_length,
			      uint8_t *const output, const size_t max_output_length,
			      size_t *const ret_output_length)
{
	struct cobs_decode decode = {0};
	size_t output_length = 0;
	bool finished = false;

	size_t i;
	for (i = 0; i < input_length; i += 1) {
		if (finished) {
			return -EINVAL;
		}

		__ASSERT(max_output_length > output_length,
			 "max_output_length=%zu, output_length=%zu", max_output_length,
			 output_length);

		bool available = false;
		const enum cobs_decode_result res =
			cobs_decode_stream(&decode, input[i], &output[output_length], &available);

		if (available) {
			output_length += 1;
		}

		switch (res) {
		case COBS_DECODE_RESULT_CONSUMED:
			break;
		case COBS_DECODE_RESULT_FINISHED:
			finished = true;
			break;
		default:
			return -EINVAL;
		}
	}

	if (i != input_length) {
		return -EINVAL;
	}
	if (!finished) {
		return -EINVAL;
	}

	*ret_output_length = output_length;
	return 0;
}
