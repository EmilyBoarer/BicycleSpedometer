
project(pico-ssd1306)

add_library(pico-ssd1306 INTERFACE)

target_sources(pico-ssd1306
        INTERFACE
        ${CMAKE_CURRENT_LIST_DIR}/src/font.h
        ${CMAKE_CURRENT_LIST_DIR}/src/ssd1306.c
        ${CMAKE_CURRENT_LIST_DIR}/src/ssd1306.h
        )