# SPDX-License-Identifier: Apache-2.0

add_library(cobs_testutils
	common.c
)
target_link_libraries(cobs_testutils PRIVATE
	COBS
)
target_include_directories(cobs_testutils PUBLIC include)
