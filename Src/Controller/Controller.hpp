#pragma once

#include "EventQueue.hpp"
#include "ht1621.hpp"

class Controller {
public:
    void init();

    void processEvent(const Event &e);

private:
    enum class State {
        Idle,
        Heating,
        Error,
    };

    State m_state = State::Idle;

    /// Табличная структура переходов
    struct Transition {
        EventType signal;
        State current;
        State next;

        void (Controller::*action)(const Event &e);
    };

    /// Методы действий (Action)
    void onStartHeating(const Event &e);

    void onStopHeating(const Event &e);

    void onTemperatureReady(const Event &e);

    void onAdjustSetpoint(const Event &e);

    void onError(const Event &e);

    void control();

    /// FSM таблица
    static inline constexpr Transition transitions[] = {
            {EventType::ButtonS4,         State::Idle,    State::Heating, &Controller::onStartHeating},
            {EventType::ButtonS4,         State::Heating, State::Idle,    &Controller::onStopHeating},
            {EventType::TemperatureReady, State::Heating, State::Heating, &Controller::onTemperatureReady},
            {EventType::ButtonS1,         State::Heating, State::Heating, &Controller::onAdjustSetpoint},
            {EventType::ButtonS2,         State::Heating, State::Heating, &Controller::onAdjustSetpoint},
            {EventType::ButtonS1,         State::Idle,    State::Idle,    &Controller::onAdjustSetpoint},
            {EventType::ButtonS2,         State::Idle,    State::Idle,    &Controller::onAdjustSetpoint},
            {EventType::ButtonS4,         State::Error,   State::Idle,    &Controller::onStopHeating},
    };

    /// Данные
    float m_setpoint = 25.0f;                   /// Уставка температуры
    float m_current = 0.0f;                     /// Текущая температура
    static constexpr float hysteresis = 0.3f;   /// Температурный гистерезис
    bool m_heater_on = false;                   /// Флаг работы обогревателя
};