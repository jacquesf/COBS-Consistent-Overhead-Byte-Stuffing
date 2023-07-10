/* SPDX-License-Identifier: Apache-2.0 */

#ifndef COBS_TESTUTILS_H
#define COBS_TESTUTILS_H

#include <cobs.h>

void unsafe_cobs_reset_encode_pool(void);

size_t cobs_encode_stream_simple(const uint8_t *const input, const size_t input_length,
				 uint8_t *const output, const size_t output_length);

int cobs_decode_stream_simple(const uint8_t *const input, const size_t input_length,
			      uint8_t *const output, const size_t max_output_length,
			      size_t *ret_output_size);

#endif /* COBS_TESTUTILS_H */
