#include "services_init.hpp"
#include "AppContext.hpp"
#include "fw_info.hpp"

void services_init(App &app) {
    static EventQueue queue;
    app.queue = &queue;

    static BeepManager beep(app.piezo);
    app.beep = &beep;

    static Controller ctrl(app.display, app.beep, app.heater);
    app.ctrl = &ctrl;

    print_fw_info(app.uart);
    app.uart->write_str("System ready.\r\n");
}
