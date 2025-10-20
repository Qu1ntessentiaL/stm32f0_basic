#pragma once

#include "EventQueue.hpp"
#include "ht1621.hpp"

class Controller {
public:
    void init() {}

    void processEvent(const Event &e);

private:
    enum class State {
        Idle,
        Heating,
        Error,
    };

    float m_setpoint = 25.0f;                   /// Уставка температуры
    float m_current = 0.0f;                     /// Текущая температура
    static constexpr float hysteresis = 0.3f;   /// Температурный гистерезис
    bool m_heater_on = false;                   /// Флаг работы обогревателя

    void control() {
        if (m_current < m_setpoint - hysteresis && !m_heater_on) {
            m_heater_on = true; // Включить обогреватель
        } else if (m_current > m_setpoint + hysteresis && m_heater_on) {
            m_heater_on = false; // Выключить обогреватель
        }
    }
};