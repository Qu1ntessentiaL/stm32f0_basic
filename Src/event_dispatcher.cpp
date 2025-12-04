#include "event_dispatcher.hpp"
#include "AppContext.hpp"
#include "Event.hpp"

void dispatch_event(App& app, const Event& e)
{
    switch (e.type)
    {
        case EventType::ButtonS3:
            if (e.value == 0) {
                app.uart->write_str("S1 pressed -> beep\r\n");
                app.piezo->setPower(500);
            }
            else if (e.value == 2) {
                app.uart->write_str("S1 released -> stop beep\r\n");
                app.piezo->setPower(0);
            }
            break;

        case EventType::Tick100ms:
            // можно добавить логирование или действия
            break;

        default:
            break;
    }

    // Передать событие контроллеру
    app.ctrl->processEvent(e);
}