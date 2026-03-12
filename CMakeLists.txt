cmake_minimum_required(VERSION 3.13)

# Pull in SDK (must be before project())
include($ENV{PICO_SDK_PATH}/external/pico_sdk_import.cmake)

project(mobileair_pico C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

# Firmware version
include(${CMAKE_SOURCE_DIR}/version.cmake)
configure_file(
    ${CMAKE_SOURCE_DIR}/src/version.h.in
    ${CMAKE_BINARY_DIR}/generated/version.h
)

# Initialize the SDK
pico_sdk_init()

add_executable(mobileair_pico
    src/main.cpp
)

target_include_directories(mobileair_pico PRIVATE
    ${CMAKE_BINARY_DIR}/generated
)

# Link pico standard library
target_link_libraries(mobileair_pico
    pico_stdlib
)

# Enable USB serial output, disable UART serial output
pico_enable_stdio_usb(mobileair_pico 1)
pico_enable_stdio_uart(mobileair_pico 0)

# Create UF2 output file (for drag-and-drop flashing)
pico_add_extra_outputs(mobileair_pico)
