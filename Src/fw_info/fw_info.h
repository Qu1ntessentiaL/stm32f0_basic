#pragma once
#include <cstdint>

#include "UsartDriver.hpp"

// Внешние макросы, подставляемые из сборки (CMake или Makefile)
#ifndef FW_GIT_TAG
#define FW_GIT_TAG "unknown"
#endif

#ifndef FW_GIT_HASH
#define FW_GIT_HASH "0000000"
#endif

struct fw_info_t {
    uint32_t magic;
    char tag[32];
    char commit[16];
};

constexpr uint32_t FW_INFO_MAGIC = 0xDEADBEEF;

extern const fw_info_t fw_info;

void print_fw_info(UsartDriver<> *uart);