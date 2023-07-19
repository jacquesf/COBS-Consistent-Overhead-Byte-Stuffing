/* SPDX-License-Identifier: Apache-2.0 */

#ifndef COBS_STREAM_H_
#define COBS_STREAM_H_

#include <stdbool.h>
#if ((KERNEL_VERSION_MAJOR) < 3) || \
(((KERNEL_VERSION_MAJOR == 3)) && ((KERNEL_VERSION_MINOR) < 1))
#include <net/buf.h>
#else
#include <zephyr/net/buf.h>
#endif

enum cobs_decode_state {
	COBS_DECODE_STATE_CODE = 0,
	COBS_DECODE_STATE_DATA,
	COBS_DECODE_STATE_FINISHED,
};

enum cobs_decode_result {
	COBS_DECODE_RESULT_CONSUMED,
	COBS_DECODE_RESULT_FINISHED,
	COBS_DECODE_RESULT_UNEXPECTED_ZERO,
	COBS_DECODE_RESULT_ERROR,
};

/**
 * State for streaming decoder.
 *
 * A zero-initialized state is a valid init-state, so you can have this as a
 * struct field without calling `cobs_decode_reset` before using it.
 */
struct cobs_decode {
	enum cobs_decode_state state;

	/**
	 * @internal Offset of the next code, relative to the next byte.
	 *
	 * Only valid during COBS_DECODE_STATE_DATA.
	 */
	uint8_t code;

	/** @internal If true, we need to write a zero at the next code byte. */
	bool pending_zero;
};

enum cobs_encode_state {
	/**
	 * 0x01 will be written.
	 *
	 * We're at a place where the 0x01 does not represent data. This is the
	 * case at the very beginning of the frame or at the code that came
	 * after a 0xff code.
	 */
	COBS_ENCODE_STATE_ZEROS_FIRSTBYTE,

	/** We still have zeros and have to write an encoded zero now. */
	COBS_ENCODE_STATE_ZEROS_CODE,
	/** We still have zeros and have to write non-zero data. */
	COBS_ENCODE_STATE_ZEROS_DATA,

	/**
	 * No more zeros and we have to write a code now.
	 *
	 * We still need codes due to the 254-byte chunking. So this is gonna
	 * be 0xFF for all codes except (maybe) the last one.
	 */
	COBS_ENCODE_STATE_NOZEROS_CODE,

	/** No more zeroes and we have to write data from the current block. */
	COBS_ENCODE_STATE_NOZEROS_DATA,
	/** The final zero at the end of the frame. */
	COBS_ENCODE_STATE_FINAL_ZERO,
	/** All data was written and the encode must not be called again. */
	COBS_ENCODE_STATE_FINISHED,
};

struct cobs_encode_zeros {
	/** @internal Position of the next zero-byte within `buf` within `struct cobs_encode`. */
	size_t next_zero;

	/**
	 * @internal Number of non-zero data left to write.
	 *
	 * This is used by the state COBS_ENCODE_STATE_ZEROS_DATA.
	 */
	size_t data_left;

	/** @internal The state that comes after COBS_ENCODE_STATE_ZEROS_DATA. */
	enum cobs_encode_state post_data_state;
};

struct cobs_encode_nozeros {
	size_t total_length;

	/**
	 * @internal How much data to write before the next code.
	 *
	 * Used by the state COBS_ENCODE_STATE_ZEROS_DATA.
	 */
	size_t data_left;
};

struct cobs_encode {
	/**
	 * @internal Current raw data source.
	 *
	 * This doesn't necessarily match the buffer that this encoder was
	 * initialized with. As soon as fragment was fully written it gets
	 * unrefed and this variable is set to the next one, if any.
	 */
	struct net_buf *buf;

	enum cobs_encode_state state;

	union {
		struct cobs_encode_zeros zeros;
		struct cobs_encode_nozeros nozeros;
	} u;
};

/**
 * Pass a single byte to the decoder.
 *
 * When this causes output, one byte will be written to `output_byte` and
 * `output_available` will be set to `true`.
 * As soon as the (required) 0-byte is received, COBS_DECODE_RESULT_FINISHED is
 * returned and you're not allowed to call this function anymore. If you do
 * anyway, COBS_DECODE_RESULT_ERROR will be returned.
 *
 * A frame is only fully received and decoded when the mentioned zero byte was
 * received and the decoder is in the finished state.
 */
enum cobs_decode_result cobs_decode_stream(struct cobs_decode *decode, uint8_t input_byte,
					   uint8_t *output_byte, bool *output_available);

/**
 * Reset decoder.
 *
 * Must be called after decoding a frame (successfully or not).
 */
static inline void cobs_decode_reset(struct cobs_decode *decode)
{
	*decode = (struct cobs_decode){
		.state = COBS_DECODE_STATE_CODE,
		.pending_zero = false,
	};
}

/**
 * Initialize stream.
 *
 * Takes ownership of `buf` meaning the reference count is not increased.
 * If you have any other references, keep in mind that the encoder detaches and
 * unrefs fragments as soon as they were processed.
 *
 * You have to call `cobs_encode_stream_free` to prevent leaking any buffers.
 */
void cobs_encode_stream_init(struct cobs_encode *encode, struct net_buf *buf);

/**
 * Abort stream.
 *
 * All future calls to `cobs_encode_stream` will return 0, so you can use that
 * function as an indicator for weather or not to send more data. That
 * simplifies user code for handling TX errors on their side.
 */
static inline void cobs_encode_stream_abort(struct cobs_encode *encode)
{
	encode->state = COBS_ENCODE_STATE_FINISHED;
}

/** Free allocated data within the stream. */
void cobs_encode_stream_free(struct cobs_encode *encode);

/**
 * Encode more data into `output`.
 *
 * There can't be any errors during encoding, so this will always succeed.
 * When there's no more data left to encode, `0` is returned.
 */
size_t cobs_encode_stream(struct cobs_encode *encode, uint8_t *output, size_t output_length);

#endif /* COBS_STREAM_H_ */
