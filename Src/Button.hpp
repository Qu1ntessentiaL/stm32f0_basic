#pragma once

#include "stm32f0xx.h"
#include "GpioDriverCT.hpp"

/**
 * @brief
 */
template<GpioPort Port, uint8_t Pin,
        uint8_t DebounceTicks = 5,
        uint16_t HoldTicks = 100>
class Button {
    using m_gpio = GpioDriverCT<Port, Pin>;
public:
    enum class State {
        Idle, DebouncePress, Pressed, DebounceRelease
    };

    enum class Event {
        None, Pressed, Released, Held
    };

    static inline void Init() {
        m_gpio::Init(m_gpio::Mode::Input);
    }

    static inline Event tick() {
        const bool pressed = m_gpio::Read() == false;
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
    static inline State m_state = State::Idle;
    static inline uint16_t m_counter = 0;
};