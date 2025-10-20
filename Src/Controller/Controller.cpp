#include "Controller.hpp"

struct EventHandlerEntry {
    EventType type;

    void (Controller::*handler)(const Event &);
};
/*
constexpr static EventHandlerEntry eventTable[] = {
        {EventType::ButtonS1,    &Controller::onButtonUp},
        {EventType::ButtonS2,  &Controller::onButtonDown},
        {EventType::ButtonS3, &Controller::onSensorError},
        {EventType::ButtonS4, &Controller::onSensorError},
};

void Controller::processEvent(const Event &e) {
    for (auto &entry: eventTable) {
        if (entry.type == e.type) {
            (this->*entry.handler)(e);
            return;
        }
    }
}
 */