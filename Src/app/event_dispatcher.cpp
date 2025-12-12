#include "event_dispatcher.hpp"
#include "AppContext.hpp"
#include "Event.hpp"

void dispatch_event(App &app, const Event &e) {
    switch (e.type) {
    case EventType::ButtonS1:
    case EventType::ButtonS2:
    case EventType::ButtonS3:
    case EventType::ButtonS4:
        if (e.value == 0) {
            // press
            app.piezo->setPower(500);
            // app.uart->write_str("Beep ON\r\n");
        }
        else if (e.value == 2) {
            // release
            app.piezo->setPower(0);
            // app.uart->write_str("Beep OFF\r\n");
        }

        break;

    case EventType::Tick100ms:
        {}
        break;

    default:
        break;
    }

    // Передать событие контроллеру
    app.ctrl->processEvent(e);
}
