cmake_minimum_required(VERSION 3.13)


add_compile_definitions(PICO_DEFAULT_UART_BAUD_RATE=921600)
add_compile_definitions(PERSISTED_CONFIG_SIZE=4096)
add_compile_definitions(PICO_XOSC_STARTUP_DELAY_MULTIPLIER=64)

set(PICO_SDK_PATH "${CMAKE_CURRENT_LIST_DIR}/pico-sdk")
set(PICO_TINYUSB_PATH "${CMAKE_CURRENT_LIST_DIR}/tinyusb")
set(PICO_PIO_USB_PATH "${CMAKE_CURRENT_LIST_DIR}/Pico-PIO-USB")
set(PICO_BOARD_HEADER_DIRS "${CMAKE_CURRENT_LIST_DIR}/src")

include(pico_sdk_import.cmake)

project(little_buddy)

pico_sdk_init()

check_and_add_pico_pio_usb_support()

add_compile_options(-Wall)


file(GLOB RECURSE SOURCES "src/*.c")

add_executable(little_buddy ${SOURCES})


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
pico_add_extra_outputs(little_buddy)
