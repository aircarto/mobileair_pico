# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
set(PICO_BOARD pico2_w CACHE STRING "Board type")

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
    src/wifi/wifi_manager.cpp
    src/wifi/wifi_credentials.cpp
    src/portal/captive_portal.cpp
    src/portal/dns_server.cpp
    src/portal/dhcp_server.c
    src/modem/modem.cpp
    src/sensors/nextpm.cpp
    src/logger.cpp
)

target_include_directories(mobileair_pico PRIVATE
    ${CMAKE_BINARY_DIR}/generated
    ${CMAKE_SOURCE_DIR}/src
)

# Link pico standard library + board info libs
target_link_libraries(mobileair_pico
    pico_stdlib
    pico_unique_id
    hardware_watchdog
    hardware_clocks
    hardware_flash
    hardware_sync
    hardware_uart
    pico_cyw43_arch_lwip_poll
    pico_lwip_http
    pico_lwip_mdns
)

# Enable USB serial output, disable UART serial output
pico_enable_stdio_usb(mobileair_pico 1)
pico_enable_stdio_uart(mobileair_pico 0)

# Create UF2 output file (for drag-and-drop flashing)
pico_add_extra_outputs(mobileair_pico)
