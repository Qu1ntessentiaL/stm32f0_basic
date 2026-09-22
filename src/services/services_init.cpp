#include "services_init.hpp"
#include "AppContext.hpp"
#include "fw_info.hpp"
#include "ds18x20_scan.hpp"
#include "RccDriver.hpp"

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

    uart->write_str("Clock: ");
    switch (RccDriver::ClockSource()) {
        case RccDriver::Sysclk::HsePll48:
            uart->write_str("HSE PLL 48MHz\r\n");
            break;
        case RccDriver::Sysclk::HsiPll48:
            uart->write_str("HSI PLL 48MHz (HSE fail)\r\n");
            break;
        default:
            uart->write_str("HSI 8MHz (PLL fail)\r\n");
            break;
    }

    uart->write_str("=====================\r\n");
}

namespace {
    void on_ds18x20_sample(uint8_t slot, int16_t temp) {
        if (DS18X20::is_error(temp) && app.uart) {
            const char *msg = "error";
            if (temp == DS18X20::TEMP_ERROR_NO_SENSOR) {
                msg = "no sensor";
            } else if (temp == DS18X20::TEMP_ERROR_CRC_FAIL) {
                msg = "CRC fail";
            }
            char line[28] = "DS18x20[";
            char *p = line + 8;
            if (slot >= 10) {
                *p++ = static_cast<char>('0' + (slot / 10));
            }
            *p++ = static_cast<char>('0' + (slot % 10));
            *p++ = ']';
            *p++ = ' ';
            while (*msg && (p < line + sizeof(line) - 3)) {
                *p++ = *msg++;
            }
            *p++ = '\r';
            *p++ = '\n';
            *p = '\0';
            app.uart->write_str(line);
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

    static Controller ctrl(app.display, app.beep, app.heater, app.red_led);
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
