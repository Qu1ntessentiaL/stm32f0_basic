#include "event_dispatcher.hpp"
#include "AppContext.hpp"

void dispatch_event(App &app, const Event &e) {
    if (app.ctrl) {
        app.ctrl->processEvent(e);
    }
}
