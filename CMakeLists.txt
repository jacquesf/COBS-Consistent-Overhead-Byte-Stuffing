
if(CONFIG_COBS)

zephyr_interface_library_named(COBS)
target_include_directories(COBS INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

zephyr_library()
zephyr_library_sources(
    cobs.c
)

zephyr_library_link_libraries(COBS)
target_link_libraries(COBS INTERFACE zephyr_interface)

endif()
