#include "fw_info.hpp"

// #pragma message ("FW_GIT_TAG = " FW_GIT_TAG)
// #pragma message ("FW_GIT_HASH = " FW_GIT_HASH)

__attribute__((used, section(".fw_info")))
const fw_info_t fw_info = {
        FW_INFO_MAGIC,
        FW_GIT_TAG,
        FW_GIT_HASH,
};
