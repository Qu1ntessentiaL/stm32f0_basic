cmake_minimum_required(VERSION 3.15)

add_library(cmsis
    STATIC
        Device/ST/STM32F0xx/Source/Templates/system_stm32f0xx.c
)

set_target_properties(cmsis
    PROPERTIES
        VERSION 1.11.6
)

target_include_directories(cmsis
    PUBLIC
        Core/Include
        Device/ST/STM32F0xx/Include
)

target_compile_definitions(cmsis
    PUBLIC
        -DSTM32F030x6
)