cmake_minimum_required(VERSION 3.13)


add_compile_definitions(PICO_DEFAULT_UART_BAUD_RATE=921600)
add_compile_definitions(PERSISTED_CONFIG_SIZE=4096)
add_compile_definitions(PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64)

set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/pico-sdk")
set(PICO_TINYUSB_PATH "${CMAKE_CURRENT_LIST_DIR}/tinyusb")
set(PICO_PIO_USB_PATH "${CMAKE_CURRENT_LIST_DIR}/Pico-PIO-USB")
set(PICO_BOARD_HEADER_DIRS "${CMAKE_CURRENT_LIST_DIR}/src/boards")

include(pico_sdk_import.cmake)

project(little_buddy)

pico_sdk_init()

check_and_add_pico_pio_usb_support()

add_compile_options(-Wall)

add_compile_options($<$<COMPILE_LANGUAGE:CXX>:-std=c++17>)

if((PICO_BOARD STREQUAL "pico") OR (PICO_BOARD STREQUAL "pico2"))
add_compile_definitions(GPIO_VALID_PINS_BASE=0b00011100011111111111111111111111)
endif()


add_executable(little_buddy
    src/main.cc
    src/red_led.cc
    src/interval_override.cc
    src/sh1107_display.cc
    src/usb_events.cc
    src/usb_host.cc
    src/hid_parser.cc
    src/usb_device.cc
    src/usb_descriptors.c
)

if((PICO_BOARD STREQUAL "pico") OR (PICO_BOARD STREQUAL "pico2"))
target_compile_definitions(little_buddy PUBLIC PICO_DEFAULT_UART_TX_PIN=16)
target_compile_definitions(little_buddy PUBLIC PICO_DEFAULT_UART_RX_PIN=17)
target_compile_definitions(little_buddy PUBLIC PICO_DEFAULT_PIO_USB_DP_PIN=0)
endif()

target_include_directories(little_buddy PRIVATE
    src
    ${PICO_PIO_USB_PATH}
)

target_link_libraries(little_buddy
    pico_stdlib
    pico_multicore
    pico_unique_id
    hardware_pio
    hardware_dma
    hardware_flash
    hardware_i2c
    tinyusb_device
    tinyusb_host
    tinyusb_board
    tinyusb_pico_pio_usb
)

# Disable Pico SDK USB stdio since we're using custom TinyUSB
pico_enable_stdio_usb(little_buddy 0)
pico_enable_stdio_uart(little_buddy 0)

pico_set_binary_type(little_buddy copy_to_ram)
if(PICO_PLATFORM STREQUAL "rp2040")
    pico_set_linker_script(little_buddy ${CMAKE_CURRENT_LIST_DIR}/little_buddy.ld)
else()
    message(FATAL_ERROR "Unknown PICO_PLATFORM.")
endif()
pico_add_extra_outputs(little_buddy)

