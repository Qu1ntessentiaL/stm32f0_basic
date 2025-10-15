cmake_minimum_required(VERSION 3.15)

set(LIBRARY_NAME chibios-nano)

#Найти все C-файлы в директории
file(GLOB_RECURSE ALL_C_SOURCES "nil/src/*.c")

# Исключить некоторые файлы
list(REMOVE_ITEM ALL_C_SOURCES
        nil/src/example.c
)

add_library(${LIBRARY_NAME} STATIC
        common/ports/ARMv6-M/compilers/GCC/chcoreasm.S
        ${ALL_C_SOURCES}
)

target_include_directories(${LIBRARY_NAME} PUBLIC
        nil/include/
        nil/templates/
        common/ports/ARM-common/
        common/ports/ARMv6-M/
        common/portability/GCC/
        common/startup/ARMCMx/devices/STM32F0xx
        license/
        oslib/include/
)

target_link_libraries(${LIBRARY_NAME} PUBLIC cmsis)

target_compile_definitions(${LIBRARY_NAME} PUBLIC
        -DSTM32F030x6
)