/* Copyright 2011, Jacques Fortier. All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted, with or without modification.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/net/buf.h>
#include <zephyr/types.h>
#include <zephyr/ztest.h>
#include <cobs.h>
#include <cobs/testutils.h>

static void verify_inplace_decoder(const uint8_t *const input_data_, const size_t input_length,
				   const uint8_t *const reference, const size_t reference_length)
{
	uint8_t *const input_data = malloc(input_length);
	zassert_not_null(input_data);
	memcpy(input_data, input_data_, input_length);

	const size_t output_length = cobs_decode_inplace(input_data, input_length);
	zassert_equal(output_length, reference_length);
	zassert_mem_equal(input_data, reference, reference_length);

	free(input_data);
}

static void roundtrip_test_runner(const void *input, const size_t length)
{
	int ret;

	const size_t encoded_buffer_length = length + length / 254 + 2;
	uint8_t *const encoded_buffer = malloc(encoded_buffer_length);
	uint8_t *const decoded_buffer = malloc(length + 1);
	memset(encoded_buffer, 0xAB, encoded_buffer_length);
	decoded_buffer[length] = 0xAB;

	const size_t encoded_length = cobs_encode(input, length, encoded_buffer);
	zassert_true(encoded_length <= (length + length / 254 + 1));
	zassert_equal(encoded_buffer[encoded_length], 0xAB);
	zassert_equal(encoded_buffer[encoded_buffer_length - 1], 0xAB);

	const size_t decoded_length = cobs_decode(encoded_buffer, encoded_length, decoded_buffer);
	zassert_equal(decoded_length, length);
	zassert_mem_equal(decoded_buffer, input, length);
	zassert_equal(decoded_buffer[length], 0xAB);

	verify_inplace_decoder(encoded_buffer, encoded_length, decoded_buffer, decoded_length);

	uint8_t *const encoded_buffer2 = malloc(encoded_buffer_length);
	uint8_t *const decoded_buffer2 = malloc(length + 1);
	memset(encoded_buffer2, 0xAB, encoded_buffer_length);
	decoded_buffer2[length] = 0xAB;

	const size_t encoded_length2 =
		cobs_encode_stream_simple(input, length, encoded_buffer2, encoded_buffer_length);
	if (length == 254 && !memchr(input, 0x00, length)) {
		/* The normal encoder writes an additional 0x01, if the
		 * previous code was 0xff and there is no data left.
		 */
		zassert_equal(encoded_length2, encoded_length);
		zassert_equal(encoded_buffer[encoded_length - 1], 0x01);
		zassert_mem_equal(encoded_buffer2, encoded_buffer, encoded_length - 1);
	} else {
		zassert_equal(encoded_length2, encoded_length + 1);
		zassert_mem_equal(encoded_buffer2, encoded_buffer, encoded_length);
	}

	size_t decoded_length2;
	ret = cobs_decode_stream_simple(encoded_buffer2, encoded_length2, decoded_buffer2,
					length + 1, &decoded_length2);
	zassert_ok(ret);
	zassert_equal(decoded_length2, length);
	zassert_mem_equal(decoded_buffer2, input, length);
	zassert_equal(decoded_buffer2[length], 0xAB);

	/* Just to double-check, compare to the output of the other
	 * implementation.
	 */
	zassert_equal(decoded_length2, decoded_length);
	zassert_mem_equal(decoded_buffer2, decoded_buffer, length);

	/* Not run if the tests fail, oh well */
	free(encoded_buffer2);
	free(decoded_buffer2);
	free(encoded_buffer);
	free(decoded_buffer);
}

ZTEST(lib_cobs_test, test_single_null)
{
	static const uint8_t buffer[] = {0};
	uint8_t encoded_buffer[] = {0xAB, 0xAB, 0xAB, 0xAB};
	static const uint8_t expected_buffer[] = {1, 1, 0xAB};
	static const uint8_t expected_buffer2[] = {1, 1, 0x00, 0xAB};

	const size_t encoded_length = cobs_encode(buffer, sizeof(buffer), encoded_buffer);
	zassert_equal(encoded_length, 2);
	zassert_mem_equal(encoded_buffer, expected_buffer, sizeof(expected_buffer));

	memset(encoded_buffer, 0xAB, sizeof(encoded_buffer));
	const size_t encoded_length2 = cobs_encode_stream_simple(
		buffer, sizeof(buffer), encoded_buffer, sizeof(encoded_buffer));
	zassert_equal(encoded_length2, 3);
	zassert_mem_equal(encoded_buffer, expected_buffer2, sizeof(expected_buffer2) - 1);
}

ZTEST(lib_cobs_test, test_hex1)
{
	static const uint8_t buffer[] = {1};
	uint8_t encoded_buffer[] = {0xAB, 0xAB, 0xAB, 0xAB};
	static const uint8_t expected_buffer[] = {2, 1, 0xAB};
	static const uint8_t expected_buffer2[] = {2, 1, 0x00, 0xAB};

	const size_t encoded_length = cobs_encode(buffer, sizeof(buffer), encoded_buffer);
	zassert_equal(encoded_length, 2);
	zassert_mem_equal(encoded_buffer, expected_buffer, sizeof(expected_buffer));

	memset(encoded_buffer, 0xAB, sizeof(encoded_buffer));
	const size_t encoded_length2 = cobs_encode_stream_simple(
		buffer, sizeof(buffer), encoded_buffer, sizeof(encoded_buffer));
	zassert_equal(encoded_length2, 3);
	zassert_mem_equal(encoded_buffer, expected_buffer2, sizeof(expected_buffer2) - 1);
}

ZTEST(lib_cobs_test, test_255_bytes_null_end)
{
	uint8_t buffer[255];
	for (unsigned int i = 0; i < sizeof(buffer) - 1; i++) {
		buffer[i] = i + 1;
	}
	buffer[sizeof(buffer) - 1] = 0;

	uint8_t encoded_buffer[258];
	uint8_t expected_buffer[258];

	expected_buffer[0] = 0xFF;
	for (int i = 0; i < 254; i++) {
		expected_buffer[i + 1] = i + 1;
	}
	expected_buffer[255] = 1;
	expected_buffer[256] = 1;
	expected_buffer[257] = 0xAB;

	encoded_buffer[257] = 0xAB;

	const size_t encoded_length = cobs_encode(buffer, sizeof(buffer), encoded_buffer);
	zassert_mem_equal(encoded_buffer, expected_buffer, sizeof(encoded_buffer));
	zassert_equal(encoded_length, 257);

	const size_t encoded_length2 = cobs_encode_stream_simple(
		buffer, sizeof(buffer), encoded_buffer, sizeof(encoded_buffer));
	zassert_equal(encoded_length2, 258);
	zassert_mem_equal(encoded_buffer, expected_buffer, sizeof(expected_buffer) - 1);
}

ZTEST(lib_cobs_test, test_hex1_rt)
{
	const uint8_t buffer[] = {1};

	roundtrip_test_runner(buffer, sizeof(buffer));
}

ZTEST(lib_cobs_test, test_single_null_rt)
{
	const uint8_t buffer[] = {0};

	roundtrip_test_runner(buffer, sizeof(buffer));
}

ZTEST(lib_cobs_test, test_two_nulls_rt)
{
	const uint8_t buffer[] = {0, 0};

	roundtrip_test_runner(buffer, sizeof(buffer));
}

ZTEST(lib_cobs_test, test_null_one_null_rt)
{
	const uint8_t buffer[] = {0, 1, 0};

	roundtrip_test_runner(buffer, sizeof(buffer));
}

ZTEST(lib_cobs_test, test_null_two_null_one_rt)
{
	const uint8_t buffer[] = {0, 2, 0, 1};

	roundtrip_test_runner(buffer, sizeof(buffer));
}

ZTEST(lib_cobs_test, test_254_bytes_rt)
{
	uint8_t buffer[254];
	for (size_t i = 0; i < sizeof(buffer); i++) {
		buffer[i] = i + 1;
	}

	roundtrip_test_runner(buffer, sizeof(buffer));
}

ZTEST(lib_cobs_test, test_254_bytes_null_end_rt)
{
	uint8_t buffer[254];
	for (size_t i = 0; i < sizeof(buffer) - 1; i++) {
		buffer[i] = i + 1;
	}
	buffer[sizeof(buffer) - 1] = 0;

	roundtrip_test_runner(buffer, sizeof(buffer));
}

ZTEST(lib_cobs_test, test_255_bytes_rt)
{
	uint8_t buffer[255];
	for (size_t i = 0; i < sizeof(buffer); i++) {
		buffer[i] = i % 254 + 1;
	}

	roundtrip_test_runner(buffer, sizeof(buffer));
}

ZTEST(lib_cobs_test, test_255_bytes_null_end_rt)
{
	uint8_t buffer[255];
	for (size_t i = 0; i < sizeof(buffer) - 1; i++) {
		buffer[i] = i + 1;
	}
	buffer[sizeof(buffer) - 1] = 0;

	roundtrip_test_runner(buffer, sizeof(buffer));
}

ZTEST(lib_cobs_test, test_256_bytes_rt)
{
	uint8_t buffer[256];
	for (size_t i = 0; i < sizeof(buffer); i++) {
		buffer[i] = i % 254 + 1;
	}

	roundtrip_test_runner(buffer, sizeof(buffer));
}

ZTEST(lib_cobs_test, test_256_bytes_null_end_rt)
{
	uint8_t buffer[256];
	for (size_t i = 0; i < sizeof(buffer) - 1; i++) {
		buffer[i] = i + 1;
	}
	buffer[sizeof(buffer) - 1] = 0;

	roundtrip_test_runner(buffer, sizeof(buffer));
}

static void before(void *const fixture)
{
	ARG_UNUSED(fixture);
	unsafe_cobs_reset_encode_pool();
}

ZTEST_SUITE(lib_cobs_test, NULL, NULL, before, NULL, NULL);
