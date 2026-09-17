cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(pico_usb_microphone C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

pico_sdk_init()

add_executable(pico_usb_microphone
    src/main.c
    src/usb_descriptors.c
)

target_include_directories(pico_usb_microphone PRIVATE ${CMAKE_CURRENT_LIST_DIR}/src)
target_compile_options(pico_usb_microphone PRIVATE -Wall -Wextra -Werror)
target_link_libraries(pico_usb_microphone PRIVATE
    pico_stdlib
    hardware_adc
    hardware_dma
    tinyusb_device
    tinyusb_board
)

pico_enable_stdio_usb(pico_usb_microphone 0)
pico_enable_stdio_uart(pico_usb_microphone 0)
pico_add_extra_outputs(pico_usb_microphone)
