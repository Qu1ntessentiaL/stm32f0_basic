#pragma once

#include "stm32f0xx.h"
#include "ht1621.hpp"

class Controller {
    enum class State {
        Idle,
        ShowCurrTemp = 1,
        SetTargTemp,
        Light,
        Count
    };

    enum class Event {
        None = 1,
        S1_ShortPress,
        S2_ShortPress,
        S3_ShortPress,
        S4_ShortPress,
        Timeout,
    };

    struct Transition {
        State from;
        Event event;
        State to;

        void (Controller::*action)();
    };

    HT1621B m_display;
    State m_curr_state;
    int8_t m_curr_temp = 0;
    int8_t m_targ_temp = 25;

    void toSetTemp() {

    }

    void incTargTemp() {

    }

    void decTargTemp() {

    }

    void EnterState(State new_state) {
        m_curr_state = new_state;
    }

public:
    constexpr static const Transition transitions[] = {
            // Текущее состояние | Событие | Новое состояние | Действие
            {State::ShowCurrTemp, Event::S1_ShortPress, State::SetTargTemp, &Controller::toSetTemp},
            {State::SetTargTemp,  Event::S3_ShortPress, State::SetTargTemp, &Controller::incTargTemp}
    };

    explicit Controller(HT1621B &display) : m_display(display), m_curr_state(State::ShowCurrTemp) {}

    void Init();

    void Tick();

    void NextScreen() {
        m_curr_state = static_cast<State>((static_cast<uint8_t>(m_curr_state) + 1) %
                                          static_cast<uint8_t>(State::Count));
    }

private:
    void OnEvent(Event e) {
        for (auto &t: transitions) {
            if (t.from == m_curr_state && t.event == e) {
                (this->*t.action)();       // Выполняем действие
                EnterState(t.to);  // Переход
                return;
            }
        }
    }
};