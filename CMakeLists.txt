cmake_minimum_required(VERSION 3.22)

set(CMAKE_SUPPRESS_DEVELOPER_WARNINGS TRUE CACHE BOOL "" FORCE)
set(CMAKE_VERBOSE_MAKEFILE OFF CACHE BOOL "" FORCE)

project(stm32f0_basic LANGUAGES C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)

# Отключаем установку для embedded
set(CMAKE_EXPORT_NO_PACKAGE_REGISTRY TRUE)
set(CMAKE_SKIP_INSTALL_ALL_DEPENDENCY TRUE)
set(CMAKE_SKIP_INSTALL_RULES TRUE)

# Явно указываем пути установки для ETL
set(CMAKE_INSTALL_LIBDIR "lib" CACHE STRING "" FORCE)
set(CMAKE_INSTALL_INCLUDEDIR "include" CACHE STRING "" FORCE)
set(CMAKE_INSTALL_BINDIR "bin" CACHE STRING "" FORCE)

# set(USE_HAL_DRIVER_FLAG ON CACHE INTERNAL "Flag indicating HAL driver is used")
# set(HAL_CONF_H_DIR ${CMAKE_SOURCE_DIR}/Src/)

set(STARTUP_FILE ${CMAKE_SOURCE_DIR}/toolchain/gcc/startup_stm32f030x6.s)

add_executable(${PROJECT_NAME}
        ${STARTUP_FILE}
)

include(Src/src.cmake)

add_subdirectory(Libraries/CMSIS)

set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(NO_STL ON CACHE BOOL "" FORCE)  # Важно для embedded!
add_subdirectory(Libraries/etl EXCLUDE_FROM_ALL)

# add_subdirectory(Libraries/os)

# stm32f1xx_hal_driver заменить на cmsis, если нет надобности в HAL
target_link_libraries(${PROJECT_NAME} PRIVATE
        cmsis
        etl::etl
        # chibios-nano
)

# target_compile_options(${PROJECT_NAME} PRIVATE
#         -include "${CMAKE_SOURCE_DIR}/Src/etl_config.h"
# )

execute_process(
        COMMAND git tag --points-at HEAD
        OUTPUT_VARIABLE GIT_TAG
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

execute_process(
        COMMAND git rev-parse --short HEAD
        OUTPUT_VARIABLE GIT_HASH
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

add_compile_definitions(
        FW_GIT_TAG="${GIT_TAG}"
        FW_GIT_HASH="${GIT_HASH}"
)

message(STATUS "FW_GIT_TAG = [${GIT_TAG}]")
message(STATUS "GIT_HASH   = [${GIT_HASH}]")

# Найти утилиты
find_program(OBJCOPY arm-none-eabi-objcopy REQUIRED)
find_program(SIZE arm-none-eabi-size REQUIRED)
find_program(OBJDUMP arm-none-eabi-objdump REQUIRED)

# Путь к elf-файлу (предполагается, что имя target — firmware.elf)
set(ELF_FILE ${CMAKE_BINARY_DIR}/${PROJECT_NAME}.elf)
set(DISASM_FILE ${CMAKE_BINARY_DIR}/${PROJECT_NAME}_disasm.s)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E echo "Generating HEX, BIN, and disassembly files..."

        # HEX файл
        COMMAND arm-none-eabi-objcopy -O ihex
        $<TARGET_FILE:${PROJECT_NAME}>
        $<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>.hex

        # BIN файл
        COMMAND arm-none-eabi-objcopy -O binary
        $<TARGET_FILE:${PROJECT_NAME}>
        $<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>.bin

        # Размер секций
        COMMAND arm-none-eabi-size $<TARGET_FILE:${PROJECT_NAME}>

        # Дизассемблирование
        COMMAND ${OBJDUMP} -d -S -M force-thumb -M reg-names-std
        $<TARGET_FILE:${PROJECT_NAME}>
        > $<TARGET_FILE_BASE_NAME:${PROJECT_NAME}>_disasm.s

        COMMENT "Post-build: HEX, BIN, and disassembly"
        VERBATIM
)

# Показывает таблицу функций (адрес, размер, тип, имя)
add_custom_target(symbol_sizes
        COMMAND ${CMAKE_OBJCOPY} -R .comment $<TARGET_FILE:${PROJECT_NAME}> tmp.elf
        COMMAND arm-none-eabi-nm -S --size-sort tmp.elf > symbol_sizes.txt
        COMMAND ${CMAKE_COMMAND} -E echo "================ Symbol sizes ================"
        COMMAND ${CMAKE_COMMAND} -E cat symbol_sizes.txt
        COMMAND ${CMAKE_COMMAND} -E rm -f tmp.elf symbol_sizes.txt
        DEPENDS ${PROJECT_NAME}
        COMMENT "Symbol sizes (sorted by size):"
)

# Показывает только секцию .text с функциями
add_custom_target(text_info
        COMMAND ${CMAKE_COMMAND} -E echo "================ .text section ================"
        COMMAND arm-none-eabi-objdump -t $<TARGET_FILE:${PROJECT_NAME}> | findstr ".text"
        DEPENDS ${PROJECT_NAME}
        COMMENT "Functions in .text section:"
)

add_custom_target(erase_flash
        COMMAND ${CMAKE_COMMAND} -E echo "Erasing flash memory..."
        COMMAND openocd -f stm32f0.cfg -c "init; reset init; stm32f0x mass_erase 0; reset halt; shutdown"
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        COMMENT "Erasing the flash memory of STM32F030K6T6"
)