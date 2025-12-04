#include "fw_info.hpp"

#pragma message ("FW_GIT_TAG = " FW_GIT_TAG)
#pragma message ("FW_GIT_HASH = " FW_GIT_HASH)

__attribute__((used, section(".fw_info")))
const fw_info_t fw_info = {
        .magic = FW_INFO_MAGIC,
        .tag = FW_GIT_TAG,
        .commit = FW_GIT_HASH,
};

void print_fw_info(UsartDriver<> *uart) {
    if (!uart) return;
    // Проверим magic
    if (fw_info.magic != 0xDEADBEEF) {
        uart->write_str("FW info not valid\r\n");
        return;
    }

    uart->write_str("=== Firmware Info ===\r\n");

    // Выводим тег без snprintf
    uart->write_str("Tag: ");
    uart->write_str(fw_info.tag);
    uart->write_str("\r\n");

    // Выводим коммит без snprintf
    uart->write_str("Commit: ");
    uart->write_str(fw_info.commit);
    uart->write_str("\r\n");

    uart->write_str("=====================\r\n");
}