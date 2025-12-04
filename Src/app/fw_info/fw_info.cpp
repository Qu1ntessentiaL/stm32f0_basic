#include "fw_info.hpp"

#pragma message ("FW_GIT_TAG = " FW_GIT_TAG)
#pragma message ("FW_GIT_HASH = " FW_GIT_HASH)

__attribute__((used, section(".fw_info")))
const fw_info_t fw_info = {
        .magic = FW_INFO_MAGIC,
        .tag = FW_GIT_TAG,
        .commit = FW_GIT_HASH,
};