#pragma once

#include "stm32f0xx.h"
#include "GpioDriver.hpp"

/**
 * @brief
 */
template<uint8_t DebounceTicks = 5,
        uint16_t HoldTicks = 100>
class Button : public GpioDriver {
public:
    Button(GPIO_TypeDef *port, uint8_t pin) : GpioDriver(port, pin) {
        Init(Mode::Input);
    }

    enum class State {
        Idle,
        DebouncePress,
        Pressed,
        DebounceRelease
    };

    enum class Event {
        None, Pressed, Released, Held
    };

    inline Event tick() {
        const bool pressed = Read() == false;
        Event e = Event::None;

        switch (m_state) {
            case State::Idle:
                if (pressed) {
                    m_state = State::DebouncePress;
                    m_counter = DebounceTicks;
                }
                break;

            case State::DebouncePress:
                if (pressed) {
                    if (--m_counter == 0) {
                        m_state = State::Pressed;
                        m_counter = HoldTicks;
                        e = Event::Pressed;
                    }
                } else {
                    m_state = State::Idle;
                }
                break;

            case State::Pressed:
                if (!pressed) {
                    m_state = State::DebounceRelease;
                    m_counter = DebounceTicks;
                } else if (--m_counter == 0) {
                    e = Event::Held;
                    m_counter = HoldTicks;
                }
                break;

            case State::DebounceRelease:
                if (!pressed) {
                    if (--m_counter == 0) {
                        m_state = State::Idle;
                        e = Event::Released;
                    }
                } else {
                    m_state = State::Pressed;
                    m_counter = HoldTicks;
                }
                break;
        }
        return e;
    }

private:
    State m_state = State::Idle;
    uint16_t m_counter = 0;
};