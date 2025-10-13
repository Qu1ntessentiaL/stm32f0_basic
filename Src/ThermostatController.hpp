#pragma once

#include "EventQueue.hpp"
#include "ht1621.hpp"

class ThermostatController {
public:
    void init() {}

    void processEvent(const Event &e) {}

private:
    float m_setpoint = 25.0f;
    float m_current = 0.0f;
    static constexpr float hysteresis = 0.3f;
    bool m_heater_on = false;

    void control() {
        if (m_current < m_setpoint - hysteresis && !m_heater_on) {
            m_heater_on = true;
            // включить реле
        } else if (m_current > m_setpoint + hysteresis && m_heater_on) {
            m_heater_on = false;
            // выключить реле
        }
    }
};