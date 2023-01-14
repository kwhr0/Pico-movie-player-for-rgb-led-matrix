cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)

include(pico_sdk_import.cmake)

project(hub C CXX ASM)

pico_sdk_init()

add_executable(hub main.c file.cpp hub75.cpp i2s.c menu.cpp usb.cpp pico_fatfs_test/fatfs/ff.c pico_fatfs_test/fatfs/tf_card.c xprintf/src/xprintf.c)

target_include_directories(hub PUBLIC ${CMAKE_CURRENT_LIST_DIR}/font8x8)
target_include_directories(hub PUBLIC ${CMAKE_CURRENT_LIST_DIR}/pico_fatfs_test/fatfs)
target_include_directories(hub PUBLIC ${CMAKE_CURRENT_LIST_DIR}/xprintf/src)

pico_generate_pio_header(hub ${CMAKE_CURRENT_LIST_DIR}/hub75.pio)
pico_generate_pio_header(hub ${CMAKE_CURRENT_LIST_DIR}/i2s.pio)
pico_generate_pio_header(hub ${CMAKE_CURRENT_LIST_DIR}/usb.pio)

pico_enable_stdio_uart(hub 0)
pico_enable_stdio_usb(hub 1)

target_link_libraries(hub pico_stdlib pico_multicore hardware_pio hardware_dma hardware_spi) 

pico_add_extra_outputs(hub)

