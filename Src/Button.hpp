#pragma once

#include "stm32f0xx.h"
#include "GpioDriver.hpp"
#include "EventQueue.hpp"

/**
 * @brief
 */
template<uint8_t DebounceTicks = 5,
        uint16_t HoldTicks = 100>
class Button : public GpioDriver {
public:
    Button(GPIO_TypeDef *port, uint8_t pin) : GpioDriver(port, pin) {
        Init(Mode::Input, OutType::OpenDrain, Pull::Up);
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

class Buttons {
public:
    Buttons(GPIO_TypeDef *portS1, uint8_t pinS1,
            GPIO_TypeDef *portS2, uint8_t pinS2,
            GPIO_TypeDef *portS3, uint8_t pinS3,
            GPIO_TypeDef *portS4, uint8_t pinS4)
            : btnS1(portS1, pinS1),
              btnS2(portS2, pinS2),
              btnS3(portS3, pinS3),
              btnS4(portS4, pinS4) {}

    void poll(EventQueue &queue) {
        using BtnEvent = typename Button<>::Event;

        auto eS1 = btnS1.tick();
        if (eS1 == BtnEvent::Pressed)
            queue.push({EventType::ButtonS1, 0});
        else if (eS1 == BtnEvent::Held)
            queue.push({EventType::ButtonS1, 1}); // Удержание
        else if (eS1 == BtnEvent::Released)
            queue.push({EventType::ButtonS1, 2});

        auto eS2 = btnS2.tick();
        if (eS2 == BtnEvent::Pressed)
            queue.push({EventType::ButtonS2, 0});
        else if (eS2 == BtnEvent::Held)
            queue.push({EventType::ButtonS2, 1});
        else if (eS2 == BtnEvent::Released)
            queue.push({EventType::ButtonS2, 2});

        auto eS3 = btnS3.tick();
        if (eS3 == BtnEvent::Pressed)
            queue.push({EventType::ButtonS3, 0});
        else if (eS3 == BtnEvent::Held)
            queue.push({EventType::ButtonS3, 1});
        else if (eS3 == BtnEvent::Released)
            queue.push({EventType::ButtonS3, 2});

        auto eS4 = btnS4.tick();
        if (eS4 == BtnEvent::Pressed)
            queue.push({EventType::ButtonS4, 0});
        else if (eS4 == BtnEvent::Held)
            queue.push({EventType::ButtonS4, 1});
        else if (eS4 == BtnEvent::Released)
            queue.push({EventType::ButtonS4, 2});
    }

private:
    Button<> btnS1;
    Button<> btnS2;
    Button<> btnS3;
    Button<> btnS4;
};