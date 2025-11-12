#include "Controller.hpp"

extern GpioDriver *red_led_ptr, *green_led_ptr;

void Controller::init() {
    m_state = State::Idle;
}

/// Действия FSM
void Controller::onStartHeating(const Event &e) {
    m_heater_on = true;
    red_led_ptr->Set();
}

void Controller::onStopHeating(const Event &e) {
    m_heater_on = false;
    red_led_ptr->Reset();
}

void Controller::onTemperatureReady(const Event &e) {
    m_current = e.value;
    control();
}

void Controller::onAdjustSetpoint(const Event &e) {
    // value = 0 -> ButtonS1 pressed (+0.5)
    // value = 1 -> ButtonS2 pressed (-0.5)
    if (e.type == EventType::ButtonS1 && e.value == 0)
        m_setpoint += 0.5f;
    else if (e.type == EventType::ButtonS2 && e.value == 0)
        m_setpoint -= 0.5f;

}

void Controller::onError(const Event &) {
    m_heater_on = false;
}

void Controller::processEvent(const Event &e) {
    for (auto &t: transitions) {
        if (t.signal == e.type && t.current == m_state) {
            (this->*t.action)(e);
            m_state = t.next;
            return;
        }
    }
    // Если нет перехода, можно добавить default обработку
}

void Controller::control() {
    if (m_current < m_setpoint - hysteresis && !m_heater_on) {
        m_heater_on = true;
    } else if (m_current > m_setpoint + hysteresis && m_heater_on) {
        m_heater_on = false;
    }
}