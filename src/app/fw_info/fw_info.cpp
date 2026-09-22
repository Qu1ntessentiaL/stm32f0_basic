#include "fw_git_defs.h"
#include "fw_info.hpp"

__attribute__((used, section(".fw_info")))
const fw_info_t fw_info = {
        FW_INFO_MAGIC,
        FW_GIT_TAG,
        FW_GIT_HASH,
};
