#include "event_dispatcher.hpp"
#include "AppContext.hpp"
#include "Event.hpp"
#include "melodies.hpp"

void dispatch_event(App &app, const Event &e) {
    // S4: мелодия целиком, пьезо на это время у MelodyPlayer.
    // S1–S3: короткий пик только через Controller → BeepManager.
    if (e.type == EventType::ButtonS4 && e.value == 0 && app.melody) {
        app.melody->play(kMortalKombatTheme, kMortalKombatThemeLength);
    }

    if (app.ctrl) {
        app.ctrl->processEvent(e);
    }
}
