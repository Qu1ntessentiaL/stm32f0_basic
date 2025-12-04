#include "app_loop.hpp"
#include "event_dispatcher.hpp"
#include "RccDriver.hpp"

#include "AppContext.hpp"

void app_loop(App &app) {
    // Low-level polling
    app.sensor->poll();
    app.uart->poll_tx();
    app.buttons->poll(*app.queue);
    app.ctrl->poll();
    app.beep->poll();

    // Timer 100 ms
    if (app.tim17->getIrqCount()) {
        app.tim17->decIrqCount();

        static uint8_t tick100 = 0;
        if (++tick100 >= 100) {
            tick100 = 0;
            app.queue->push({EventType::Tick100ms, 0});
        }
    }

    // Event processing
    if (auto e = app.queue->pop()) {
        dispatch_event(app, *e);
    }

    // Watchdog
    RccDriver::IWDG_Reload();
}
