cmake_minimum_required(VERSION 3.13)

include(pico_sdk_import.cmake)

project(test_project C CXX ASM)
set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
pico_sdk_init()

add_executable(spedo
  spedo.c
)

pico_enable_stdio_usb(spedo 1)
pico_enable_stdio_uart(spedo 1)

pico_add_extra_outputs(spedo)

add_subdirectory(extern/pico-ssd1306)

target_link_libraries(spedo pico_stdlib hardware_gpio hardware_i2c pico-ssd1306)