#include "services_init.hpp"
#include "AppContext.hpp"
#include "fw_info.hpp"
#include "ds18x20_scan.hpp"

extern App app;

void print_fw_info(UsartDriver<> *uart) {
    if (!uart) return;
    // Проверим magic
    if (fw_info.magic != 0xDEADBEEF) {
        uart->write_str("\r\nFW info not valid\r\n");
        return;
    }

    uart->write_str("\r\n=== Firmware Info ===\r\n");

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

namespace {
    void on_ds18x20_sample(uint8_t slot, int16_t temp) {
        if (DS18X20::is_error(temp) && app.uart) {
            app.uart->write_str("DS18x20[");
            app.uart->write_int(slot);
            app.uart->write_str("] ");
            if (temp == DS18X20::TEMP_ERROR_NO_SENSOR) {
                app.uart->write_str("no sensor\r\n");
            } else if (temp == DS18X20::TEMP_ERROR_CRC_FAIL) {
                app.uart->write_str("CRC fail\r\n");
            } else {
                app.uart->write_str("error\r\n");
            }
        }
        if (app.queue) {
            app.queue->push({EventType::TemperatureReady, temp, slot});
        }
    }
}

void services_init(App &app) {
    static BeepManager beep(app.piezo);
    app.beep = &beep;

    static MelodyPlayer melody(app.piezo);
    app.melody = &melody;

    static Controller ctrl(app.display, app.beep, app.heater);
    app.ctrl = &ctrl;
    ctrl.init();

    if (app.sensor) {
        app.sensor->set_sample_callback(on_ds18x20_sample);
        if constexpr (DS18X20_BUS_SCAN_ENABLED) {
            ds18x20_scan_bus(app.uart);
            app.sensor->rearm();
        }
    }

    if (app.uart) {
        print_fw_info(app.uart);
        app.uart->flush();
        app.uart->write_str("System ready.\r\n");
        app.uart->flush();
    }
}
