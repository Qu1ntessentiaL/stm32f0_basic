# Пишет fw_git_defs.h с тегом и коротким хешем текущего HEAD.
# Вызывается на configure и перед каждой сборкой (copy_if_different).

if(NOT DEFINED SOURCE_DIR OR NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "GenerateFwGit.cmake: SOURCE_DIR and OUTPUT_FILE required")
endif()

set(FW_GIT_TAG "untagged")
set(FW_GIT_HASH "0000000")

execute_process(
        COMMAND git tag --points-at HEAD
        WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_VARIABLE _tag
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
)
execute_process(
        COMMAND git rev-parse --short HEAD
        WORKING_DIRECTORY "${SOURCE_DIR}"
        OUTPUT_VARIABLE _hash
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        RESULT_VARIABLE _hash_rv
)

if(_hash_rv EQUAL 0 AND NOT _hash STREQUAL "")
    set(FW_GIT_HASH "${_hash}")
endif()
if(NOT _tag STREQUAL "")
    set(FW_GIT_TAG "${_tag}")
endif()

get_filename_component(_outdir "${OUTPUT_FILE}" DIRECTORY)
file(MAKE_DIRECTORY "${_outdir}")

set(_content "#pragma once\n#define FW_GIT_TAG \"${FW_GIT_TAG}\"\n#define FW_GIT_HASH \"${FW_GIT_HASH}\"\n")
set(_tmp "${OUTPUT_FILE}.tmp")
file(WRITE "${_tmp}" "${_content}")
execute_process(COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_tmp}" "${OUTPUT_FILE}")
file(REMOVE "${_tmp}")
