#pragma once

#include <cstdint>
#include "tinyfsm.hpp"

// Определяем события
struct ButtonPressed : tinyfsm::Event {
};
struct ButtonReleased : tinyfsm::Event {
};
struct TimerTick : tinyfsm::Event {
};
struct ShortPress : tinyfsm::Event {
};
struct LongPress : tinyfsm::Event {
};

// Базовый класс FSM
class ButtonFsm : public tinyfsm::Fsm<ButtonFsm> {
public:
    static uint16_t press_time; // Счетчик времени удержания

    void react(tinyfsm::Event const &) {}

    virtual void react(ButtonPressed const &) {}

    virtual void react(ButtonReleased const &) {}

    virtual void react(TimerTick const &) {}

    void entry() {}

    void exit() {}
};