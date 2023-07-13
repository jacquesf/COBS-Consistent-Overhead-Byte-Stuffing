# SPDX-License-Identifier: Apache-2.0

if(CONFIG_COBS)

zephyr_interface_library_named(COBS)
target_include_directories(COBS INTERFACE include)

zephyr_library()
zephyr_library_sources(
    cobs.c
    stream.c
)

zephyr_library_link_libraries(COBS)
target_link_libraries(COBS INTERFACE zephyr_interface)

endif()
