/* Copyright 2011, Jacques Fortier. All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted, with or without modification.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <zephyr/types.h>
#include <zephyr/ztest.h>
#include <cobs.h>

#define ROUNDTRIP_TEST_RUNNER(input, length)                                                       \
	do {                                                                                       \
		size_t encoded_buffer_length = (length) + (length) / 254 + 2;                      \
		uint8_t *encoded_buffer = malloc(encoded_buffer_length);                           \
		uint8_t *decoded_buffer = malloc(length + 1);                                      \
		memset(encoded_buffer, 0xAB, encoded_buffer_length);                               \
		decoded_buffer[length] = 0xAB;                                                     \
                                                                                                   \
		size_t encoded_length = cobs_encode(input, length, encoded_buffer);                \
		zassert_true(encoded_length <= (length + (length) / 254 + 1));                     \
		zassert_equal(encoded_buffer[encoded_length], 0xAB);                               \
		zassert_equal(encoded_buffer[encoded_buffer_length - 1], 0xAB);                    \
                                                                                                   \
		size_t decoded_length =                                                            \
			cobs_decode(encoded_buffer, encoded_length, decoded_buffer);               \
                                                                                                   \
		/*      printf("Original:" );                                                      \
			for( size_t i = 0; i < length; i++ ) printf(" %02X", (input)[i] );         \
			printf("\nEncoded:" );                                                     \
			for( size_t i = 0; i < encoded_buffer_length; i++ ) printf(" %02X",        \
		   encoded_buffer[i] ); printf("\nDecoded:" ); for( size_t i = 0; i < length; i++  \
		   ) printf(" %02X", decoded_buffer[i] ); printf("\n" ); */                        \
		zassert_equal(decoded_length, length);                                             \
		zassert_mem_equal(decoded_buffer, input, length);                                  \
		zassert_equal(decoded_buffer[length], 0xAB);                                       \
                                                                                                   \
		free(encoded_buffer); /* Not run if the tests fail, oh well */                     \
		free(decoded_buffer);                                                              \
	} while (0)

ZTEST(lib_cobs_test, test_single_null)
{
	uint8_t buffer[] = {0};
	uint8_t encoded_buffer[3] = {0xAB, 0xAB, 0xAB};
	uint8_t expected_buffer[3] = {1, 1, 0xAB};

	size_t encoded_length = cobs_encode(buffer, sizeof(buffer), encoded_buffer);

	zassert_equal(encoded_length, 2);
	zassert_mem_equal(encoded_buffer, expected_buffer, sizeof(encoded_buffer));
}

ZTEST(lib_cobs_test, test_hex1)
{
	uint8_t buffer[] = {1};
	uint8_t encoded_buffer[3] = {0xAB, 0xAB, 0xAB};
	uint8_t expected_buffer[3] = {2, 1, 0xAB};

	size_t encoded_length = cobs_encode(buffer, sizeof(buffer), encoded_buffer);

	zassert_equal(encoded_length, 2);
	zassert_mem_equal(encoded_buffer, expected_buffer, sizeof(encoded_buffer));
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

	size_t encoded_length = cobs_encode(buffer, sizeof(buffer), encoded_buffer);

	zassert_mem_equal(encoded_buffer, expected_buffer, sizeof(encoded_buffer));
	zassert_equal(encoded_length, 257);
}

ZTEST(lib_cobs_test, test_hex1_rt)
{
	uint8_t buffer[] = {1};

	ROUNDTRIP_TEST_RUNNER(buffer, sizeof(buffer));
}

ZTEST(lib_cobs_test, test_single_null_rt)
{
	uint8_t buffer[] = {0};

	ROUNDTRIP_TEST_RUNNER(buffer, sizeof(buffer));
}

ZTEST(lib_cobs_test, test_two_nulls_rt)
{
	uint8_t buffer[] = {0, 0};

	ROUNDTRIP_TEST_RUNNER(buffer, sizeof(buffer));
}

ZTEST(lib_cobs_test, test_null_one_null_rt)
{
	uint8_t buffer[] = {0, 1, 0};

	ROUNDTRIP_TEST_RUNNER(buffer, sizeof(buffer));
}

ZTEST(lib_cobs_test, test_null_two_null_one_rt)
{
	uint8_t buffer[] = {0, 2, 0, 1};

	ROUNDTRIP_TEST_RUNNER(buffer, sizeof(buffer));
}

ZTEST(lib_cobs_test, test_254_bytes_rt)
{
	uint8_t buffer[254];
	for (unsigned int i = 0; i < sizeof(buffer); i++) {
		buffer[i] = i + 1;
	}

	ROUNDTRIP_TEST_RUNNER(buffer, sizeof(buffer));
}

ZTEST(lib_cobs_test, test_254_bytes_null_end_rt)
{
	uint8_t buffer[254];
	for (unsigned int i = 0; i < sizeof(buffer) - 1; i++) {
		buffer[i] = i + 1;
	}
	buffer[sizeof(buffer) - 1] = 0;

	ROUNDTRIP_TEST_RUNNER(buffer, sizeof(buffer));
}

ZTEST(lib_cobs_test, test_255_bytes_rt)
{
	uint8_t buffer[255];
	for (unsigned int i = 0; i < sizeof(buffer); i++) {
		buffer[i] = i % 254 + 1;
	}

	ROUNDTRIP_TEST_RUNNER(buffer, sizeof(buffer));
}

ZTEST(lib_cobs_test, test_255_bytes_null_end_rt)
{
	uint8_t buffer[255];
	for (unsigned int i = 0; i < sizeof(buffer) - 1; i++) {
		buffer[i] = i + 1;
	}
	buffer[sizeof(buffer) - 1] = 0;

	ROUNDTRIP_TEST_RUNNER(buffer, sizeof(buffer));
}

ZTEST(lib_cobs_test, test_256_bytes_rt)
{
	uint8_t buffer[256];
	for (unsigned int i = 0; i < sizeof(buffer); i++) {
		buffer[i] = i % 254 + 1;
	}

	ROUNDTRIP_TEST_RUNNER(buffer, sizeof(buffer));
}

ZTEST(lib_cobs_test, test_256_bytes_null_end_rt)
{
	uint8_t buffer[256];
	for (unsigned int i = 0; i < sizeof(buffer) - 1; i++) {
		buffer[i] = i + 1;
	}
	buffer[sizeof(buffer) - 1] = 0;

	ROUNDTRIP_TEST_RUNNER(buffer, sizeof(buffer));
}

ZTEST_SUITE(lib_cobs_test, NULL, NULL, NULL, NULL, NULL);
