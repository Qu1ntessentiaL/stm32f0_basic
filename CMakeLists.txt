# STM32F030 Firmware Project
cmake_minimum_required(VERSION 3.15)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Project Configuration
project(stm32f0_basic
        VERSION 1.0.0
        LANGUAGES C CXX ASM
)

# C/C++ Standards
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Build type
if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE MinSizeRel CACHE STRING "Build type" FORCE)
endif ()

# LTO выключен: быстрее сборка и читаемый map. На размер это не «оптимизация».
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)

# CMake module path
list(APPEND CMAKE_MODULE_PATH ${CMAKE_SOURCE_DIR}/cmake)
include(CompilerFlags)

# ===================
# Memory layout alias
# ===================
# add_subdirectory(toolchain/mem_sections)

# =====================
# Third-Party Libraries
# =====================
include(ThirdParty)
configure_all_third_party_libraries()

# ===============
# Main Executable
# ===============

# Startup and system files
set(STARTUP_FILE ${CMAKE_SOURCE_DIR}/toolchain/gcc/startup_stm32f030x6.s)

# Create executable
add_executable(${PROJECT_NAME}
        ${STARTUP_FILE}
)

# ==================
# Project Components
# ==================

# Application sources
add_subdirectory(src)

target_link_options(${PROJECT_NAME} PRIVATE
        "-Wl,-Map=${CMAKE_BINARY_DIR}/${PROJECT_NAME}.map"
)

# ==============
# Link Libraries
# ==============
target_link_libraries(${PROJECT_NAME}
    PUBLIC
        cmsis
        etl::etl
)

# Optional: ETL support
if (NO_STL)
    target_compile_definitions(${PROJECT_NAME}
        PUBLIC
            ETL_NO_STL
    )
endif ()

# ============
# Link options
# ============

# Отключаем стандартные конструкторы
# target_link_options(stm32f0_basic
#     PRIVATE
#         -nostartfiles
#         -Wl,--undefined=main
# )

# =================
# Add firmware info
# =================
# Хеш пишется в generated/fw_git_defs.h на каждый ninja, не только на cmake.
# copy_if_different: fw_info.cpp пересобирается, только если HEAD реально сменился.

set(FW_GIT_DEFS ${CMAKE_BINARY_DIR}/generated/fw_git_defs.h)
execute_process(
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DOUTPUT_FILE=${FW_GIT_DEFS}
            -P ${CMAKE_SOURCE_DIR}/cmake/GenerateFwGit.cmake
)
add_custom_target(fw_git_refresh
        COMMAND ${CMAKE_COMMAND}
            -DSOURCE_DIR=${CMAKE_SOURCE_DIR}
            -DOUTPUT_FILE=${FW_GIT_DEFS}
            -P ${CMAKE_SOURCE_DIR}/cmake/GenerateFwGit.cmake
        BYPRODUCTS ${FW_GIT_DEFS}
        COMMENT "Refresh firmware git identity"
)
add_dependencies(${PROJECT_NAME} fw_git_refresh)
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_BINARY_DIR}/generated)

file(STRINGS ${FW_GIT_DEFS} _fw_git_lines)
message(STATUS "Firmware git: ${_fw_git_lines}")

# ===================
# Post-Build Commands
# ===================
include(PostBuild)

# Add post-build commands (HEX, BIN, disassembly)
add_firmware_post_build_commands(${PROJECT_NAME})

# Add analysis targets
add_symbol_sizes_postbuild(${PROJECT_NAME})
add_symbol_addresses_postbuild(${PROJECT_NAME})
add_text_info_target(${PROJECT_NAME})

# Add flash operations
add_erase_flash_target()
add_flash_target(${PROJECT_NAME})

# =============
# Build Summary
# =============
message(STATUS "")
message(STATUS "======= Build Configuration =======")
message(STATUS "Project:      ${PROJECT_NAME} v${PROJECT_VERSION}")
message(STATUS "Generator:    ${CMAKE_GENERATOR}")
message(STATUS "Binary dir:   ${CMAKE_BINARY_DIR}")
string(TOUPPER "${CMAKE_BUILD_TYPE}" _stm32_bt)
message(STATUS "Build Type:   ${CMAKE_BUILD_TYPE}")
message(STATUS "C flags:      ${CMAKE_C_FLAGS} ${CMAKE_C_FLAGS_${_stm32_bt}}")
message(STATUS "Toolchain:    ${CMAKE_C_COMPILER_ID}")
message(STATUS "C Standard:   ${CMAKE_C_STANDARD}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "===================================")
message(STATUS "")
