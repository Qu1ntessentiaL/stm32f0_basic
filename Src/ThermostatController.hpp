#pragma once

#include "EventQueue.hpp"
#include "ht1621.hpp"

class ThermostatController {
public:
    void init() {

    }

    void processEvent(const Event &ev);

private:
    float setpoint_ = 25.0f;
    float current_ = 0.0f;
    bool heater_on_ = false;

    void control() {
        if (current_ < setpoint_ - 0.3f && !heater_on_) {
            heater_on_ = true;
            // включить реле
        } else if (current_ > setpoint_ + 0.3f && heater_on_) {
            heater_on_ = false;
            // выключить реле
        }
    }
};