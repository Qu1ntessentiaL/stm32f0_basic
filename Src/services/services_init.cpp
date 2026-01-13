#include "services_init.hpp"
#include "AppContext.hpp"
#include "fw_info.hpp"

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

void services_init(App &app) {
    // EventQueue already initialized in hardware_init

    static BeepManager beep(app.piezo);
    app.beep = &beep;

    static Controller ctrl(app.display, app.beep, app.heater);
    app.ctrl = &ctrl;

    print_fw_info(app.uart);
    app.uart->flush();  // Wait for all TX data to be sent before continuing
    
    app.uart->write_str("System ready.\r\n");
}
