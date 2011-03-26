/* Copyright 2011, Jacques Fortier. All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted, with or without modification.
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "cobs.h"

#define ASSERT_EQUAL_LUINT(value, expected) \
    do {\
        if( (value) != (expected) ) { \
            printf( "%30s: Failed, %s != %s. Expected %lu, got %lu\n", __func__, #value, #expected, (unsigned long)(expected), (unsigned long)(value) ); \
            return false; \
        } \
    } while(0)


#define ASSERT_LTE_LUINT(value, expected) \
    do {\
        if( !( (value) <= (expected) ) ) { \
            printf( "%30s: Failed, %s > %s. Expected <= %lu, got %lu\n", __func__, #value, #expected, (unsigned long)(expected), (unsigned long)(value) ); \
            return false; \
        } \
    } while(0)

#define ASSERT_EQUAL_UINT8_HEX(value, expected) \
    do {\
        if( (value) != (expected) ) { \
            printf( "%30s: Failed, %s != %s. Expected 0x%02X, got 0x%02X\n", __func__, #value, #expected, (expected), (value) ); \
            return false; \
        } \
    } while(0)

#define ASSERT_EQUAL_MEM(value, expected, length) \
    do {\
        for( size_t i = 0; i < (length); i++ ) { \
            if( (value)[i] != (expected)[i] ) { \
                printf( "%30s: Failed, %s != %s. Expected %s[%lu] = 0x%02X, got 0x%02X\n", __func__, #value, #expected, #value, i, (expected)[i], (value)[i] ); \
                return false; \
            } \
        } \
   } while(0)

#define ROUNDTRIP_TEST_RUNNER( input, length ) \
    do { \
        size_t encoded_buffer_length = (length) + (length) / 254 + 2; \
        uint8_t *encoded_buffer = malloc(encoded_buffer_length); \
        uint8_t *decoded_buffer = malloc(length + 1); \
        memset( encoded_buffer, 0xAB, encoded_buffer_length ); \
        decoded_buffer[length] = 0xAB; \
\
        size_t encoded_length = cobs_encode( input, length, encoded_buffer ); \
        ASSERT_LTE_LUINT(encoded_length, length + (length)/254 + 1); \
        ASSERT_EQUAL_UINT8_HEX( encoded_buffer[encoded_length], 0xAB ); \
        ASSERT_EQUAL_UINT8_HEX( encoded_buffer[encoded_buffer_length - 1], 0xAB ); \
\
        size_t decoded_length = cobs_decode( encoded_buffer, encoded_length, decoded_buffer ); \
\
/*      printf("Original:" ); \
        for( size_t i = 0; i < length; i++ ) printf(" %02X", (input)[i] ); \
        printf("\nEncoded:" ); \
        for( size_t i = 0; i < encoded_buffer_length; i++ ) printf(" %02X", encoded_buffer[i] ); \
        printf("\nDecoded:" ); \
        for( size_t i = 0; i < length; i++ ) printf(" %02X", decoded_buffer[i] ); \
        printf("\n" ); */\
        ASSERT_EQUAL_LUINT( decoded_length, length ); \
        ASSERT_EQUAL_MEM( decoded_buffer, input, length ); \
        ASSERT_EQUAL_UINT8_HEX( decoded_buffer[length], 0xAB ); \
\
        free(encoded_buffer); /* Not run if the tests fail, oh well */ \
        free(decoded_buffer); \
\
        return true; \
    } while(0)

bool test_single_null( void )
{
    uint8_t buffer[] = { 0 };
    uint8_t encoded_buffer[3] = { 0xAB, 0xAB, 0xAB };
    uint8_t expected_buffer[3] = { 1, 1, 0xAB };

    size_t encoded_length = cobs_encode( buffer, sizeof(buffer), encoded_buffer );

    ASSERT_EQUAL_LUINT( encoded_length, 2 );
    ASSERT_EQUAL_MEM( encoded_buffer, expected_buffer, sizeof(encoded_buffer) );

    return true;
}

bool test_hex1( void )
{
    uint8_t buffer[] = { 1 };
    uint8_t encoded_buffer[3] = { 0xAB, 0xAB, 0xAB };
    uint8_t expected_buffer[3] = { 2, 1, 0xAB };

    size_t encoded_length = cobs_encode( buffer, sizeof(buffer), encoded_buffer );

    ASSERT_EQUAL_LUINT( encoded_length, 2 );
    ASSERT_EQUAL_MEM( encoded_buffer, expected_buffer, sizeof(encoded_buffer) );

    return true;
}

bool test_255_bytes_null_end( void )
{
    uint8_t buffer[255];
    for( unsigned int i = 0; i < sizeof(buffer) - 1; i++ )
    {
        buffer[i] = i + 1;
    }
    buffer[sizeof(buffer) - 1] = 0;

    uint8_t encoded_buffer[257];
    uint8_t expected_buffer[257];

    expected_buffer[0] = 0xFF;
    for( int i = 0; i < 254; i++ )
    {
        expected_buffer[i + 1] = i + 1;
    }
    expected_buffer[256] = 1;
    expected_buffer[257] = 0xAB;

    encoded_buffer[256] = 0xAB;

    size_t encoded_length = cobs_encode( buffer, sizeof(buffer), encoded_buffer );

    ASSERT_EQUAL_MEM( encoded_buffer, expected_buffer, sizeof(encoded_buffer) );
    ASSERT_EQUAL_LUINT( encoded_length, 256 );

    return true;
}

bool test_hex1_rt( void )
{
    uint8_t buffer[] = { 1 };

    ROUNDTRIP_TEST_RUNNER( buffer, sizeof(buffer) );
}

bool test_single_null_rt( void )
{
    uint8_t buffer[] = { 0 };

    ROUNDTRIP_TEST_RUNNER( buffer, sizeof(buffer) );
}

bool test_two_nulls_rt( void )
{
    uint8_t buffer[] = { 0, 0 };

    ROUNDTRIP_TEST_RUNNER( buffer, sizeof(buffer) );
}

bool test_null_one_null_rt( void )
{
    uint8_t buffer[] = { 0, 1, 0 };

    ROUNDTRIP_TEST_RUNNER( buffer, sizeof(buffer) );
}

bool test_null_two_null_one_rt( void )
{
    uint8_t buffer[] = { 0, 2, 0, 1 };

    ROUNDTRIP_TEST_RUNNER( buffer, sizeof(buffer) );
}

bool test_254_bytes_rt( void )
{
    uint8_t buffer[254];
    for( unsigned int i = 0; i < sizeof(buffer); i++ )
    {
        buffer[i] = i + 1;
    }

    ROUNDTRIP_TEST_RUNNER( buffer, sizeof(buffer) );
}

bool test_254_bytes_null_end_rt( void )
{
    uint8_t buffer[254];
    for( unsigned int i = 0; i < sizeof(buffer) - 1; i++ )
    {
        buffer[i] = i + 1;
    }
    buffer[sizeof(buffer) - 1] = 0;

    ROUNDTRIP_TEST_RUNNER( buffer, sizeof(buffer) );
}

bool test_255_bytes_rt( void )
{
    uint8_t buffer[255];
    for( unsigned int i = 0; i < sizeof(buffer); i++ )
    {
        buffer[i] = i % 254 + 1;
    }

    ROUNDTRIP_TEST_RUNNER( buffer, sizeof(buffer) );
}

bool test_255_bytes_null_end_rt( void )
{
    uint8_t buffer[255];
    for( unsigned int i = 0; i < sizeof(buffer) - 1; i++ )
    {
        buffer[i] = i + 1;
    }
    buffer[sizeof(buffer) - 1] = 0;

    ROUNDTRIP_TEST_RUNNER( buffer, sizeof(buffer) );
}

bool test_256_bytes_rt( void )
{
    uint8_t buffer[256];
    for( unsigned int i = 0; i < sizeof(buffer); i++ )
    {
        buffer[i] = i % 254 + 1;
    }

    ROUNDTRIP_TEST_RUNNER( buffer, sizeof(buffer) );
}

bool test_256_bytes_null_end_rt( void )
{
    uint8_t buffer[256];
    for( unsigned int i = 0; i < sizeof(buffer) - 1; i++ )
    {
        buffer[i] = i + 1;
    }
    buffer[sizeof(buffer) - 1] = 0;

    ROUNDTRIP_TEST_RUNNER( buffer, sizeof(buffer) );
}

int main(int argc, char*argv[])
{
    test_single_null();
    test_hex1();

    test_hex1_rt();
    test_single_null_rt();
    test_two_nulls_rt();
    test_null_one_null_rt();
    test_null_two_null_one_rt();
    test_254_bytes_rt();
    test_254_bytes_null_end_rt();
    test_255_bytes_rt();
    test_255_bytes_null_end_rt();
    test_256_bytes_rt();
    test_256_bytes_null_end_rt();

    return 0;
}
