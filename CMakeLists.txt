cmake_minimum_required(VERSION 3.22)

set(CMAKE_VERBOSE_MAKEFILE OFF CACHE BOOL "" FORCE)

set(CMAKE_PROJECT_NAME stm32f0_basic)
project(${CMAKE_PROJECT_NAME} LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

# set(USE_HAL_DRIVER_FLAG ON CACHE INTERNAL "Flag indicating HAL driver is used")
# set(HAL_CONF_H_DIR ${CMAKE_SOURCE_DIR}/Src/)

set(STARTUP_FILE ${CMAKE_SOURCE_DIR}/toolchain/gcc/startup_stm32f030x6.s)

add_executable(${PROJECT_NAME}
        ${STARTUP_FILE}
)

include(Src/app.cmake)

add_subdirectory(Drivers/CMSIS)

# stm32f1xx_hal_driver заменить на cmsis, если нет надобности в HAL
target_link_libraries(${PROJECT_NAME} PRIVATE
        cmsis
)

# Найти objdump в PATH
find_program(OBJDUMP arm-none-eabi-objdump REQUIRED)

# Путь к elf-файлу (предполагается, что имя target — firmware.elf)
set(ELF_FILE ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.elf)
set(DISASM_FILE ${CMAKE_BINARY_DIR}/${PROJECT_NAME}_disasm.s)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Generating HEX and BIN files..."

        # Переименовываем .elf (опционально, если нужно сохранить оригинальный файл)
        COMMAND ${CMAKE_COMMAND} -E rename
        $<TARGET_FILE:${PROJECT_NAME}>
        $<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>.elf

        # Конвертируем ELF -> HEX (Intel HEX)
        COMMAND arm-none-eabi-objcopy -O ihex
        $<TARGET_FILE:${PROJECT_NAME}>
        $<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>.hex

        # Конвертируем ELF -> BIN (Raw binary)
        COMMAND arm-none-eabi-objcopy -O binary
        $<TARGET_FILE:${PROJECT_NAME}>
        $<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>.bin

        # Можно добавить вывод размера файла (опционально)
        COMMAND arm-none-eabi-size $<TARGET_FILE:${PROJECT_NAME}>

        COMMAND ${OBJDUMP} -d -S -M force-thumb -M reg-names-std ${ELF_FILE} > ${DISASM_FILE}
        COMMENT "Disassembling ${PROJECT_NAME}.elf to ${PROJECT_NAME}_disasm.s"
        DEPENDS ${PROJECT_NAME}.elf
        VERBATIM
)

add_custom_target(erase_flash
        COMMAND ${CMAKE_COMMAND} -E echo "Erasing flash memory..."
        COMMAND openocd -f stm32f0.cfg -c "init; reset init; stm32f0x mass_erase 0; reset halt; shutdown"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Erasing the flash memory of STM32F030K6T6"
)