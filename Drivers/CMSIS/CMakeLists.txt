cmake_minimum_required(VERSION 3.15)

set(STM32_FAMILY STM32F0)

add_library(cmsis STATIC
        Device/ST/${STM32_FAMILY}xx/Source/Templates/system_stm32f0xx.c
)

target_include_directories(cmsis PUBLIC
        Core/Include
        Device/ST/${STM32_FAMILY}xx/Include
        ${CMAKE_SOURCE_DIR}/Drivers/${STM32_FAMILY}xx_HAL_Driver/Inc
        ${CMAKE_SOURCE_DIR}/Src
)

target_compile_definitions(cmsis PUBLIC
        -DSTM32F030x6
        # -DUSE_HAL_DRIVER
        # -DUSE_FULL_ASSERT
)