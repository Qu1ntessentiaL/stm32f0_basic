set(CMAKE_SYSTEM_NAME               Generic)
set(CMAKE_SYSTEM_PROCESSOR          arm)

set(CMAKE_C_COMPILER_FORCED TRUE)
set(CMAKE_CXX_COMPILER_FORCED TRUE)
set(CMAKE_C_COMPILER_ID GNU)
set(CMAKE_CXX_COMPILER_ID GNU)

# arm-none-eabi- must be on PATH
set(TOOLCHAIN_PREFIX                arm-none-eabi-)

set(CMAKE_C_COMPILER                ${TOOLCHAIN_PREFIX}gcc)
set(CMAKE_ASM_COMPILER              ${CMAKE_C_COMPILER})
set(CMAKE_CXX_COMPILER              ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_LINKER                    ${TOOLCHAIN_PREFIX}g++)
set(CMAKE_OBJCOPY                   ${TOOLCHAIN_PREFIX}objcopy)
set(CMAKE_SIZE                      ${TOOLCHAIN_PREFIX}size)

set(CMAKE_EXECUTABLE_SUFFIX_ASM     ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_C       ".elf")
set(CMAKE_EXECUTABLE_SUFFIX_CXX     ".elf")

set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

# Только ISA и общие опции. -O / -g / NDEBUG задаёт cmake/CompilerFlags.cmake.
set(STM32_CPU_FLAGS "-mcpu=cortex-m0 -mthumb")
set(STM32_COMMON_FLAGS "${STM32_CPU_FLAGS} -Wall -Wextra -Wpedantic -Wno-psabi -fdata-sections -ffunction-sections -fno-unwind-tables -fno-asynchronous-unwind-tables")

set(CMAKE_C_FLAGS "${STM32_COMMON_FLAGS}" CACHE STRING "Common C flags" FORCE)
set(CMAKE_CXX_FLAGS "${STM32_COMMON_FLAGS} -fno-rtti -fno-exceptions -fno-threadsafe-statics" CACHE STRING "Common C++ flags" FORCE)
set(CMAKE_ASM_FLAGS "${STM32_CPU_FLAGS} -x assembler-with-cpp -MMD -MP" CACHE STRING "Common ASM flags" FORCE)

# Map-файл добавляется после project(): здесь CMAKE_PROJECT_NAME ещё пуст.
set(CMAKE_C_LINK_FLAGS "${STM32_CPU_FLAGS}")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -T \"${CMAKE_CURRENT_LIST_DIR}/stm32f030k6tx_flash.ld\"")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} --specs=nano.specs")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--gc-sections")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--start-group -lc -lm -Wl,--end-group")
set(CMAKE_C_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--print-memory-usage")

set(CMAKE_CXX_LINK_FLAGS "${CMAKE_C_LINK_FLAGS} -Wl,--start-group -lstdc++ -lsupc++ -Wl,--end-group")
