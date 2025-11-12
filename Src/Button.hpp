#pragma once

#include "stm32f0xx.h"
#include "GpioDriver.hpp"
#include "SystemTime.h"
#include "EventQueue.hpp"

/**
 * @brief Кнопка с программным антидребезгом и детектором удержания.
 *
 * @tparam DebounceMs   Время (мс), в течение которого состояние должно оставаться
 *                      неизменным, чтобы считаться устойчивым.
 * @tparam HoldMs       Период (мс) генерации событий удержания.
 */
template<uint16_t DebounceMs = 30,
         uint16_t HoldMs = 100>
class Button : public GpioDriver {
public:
    Button(GPIO_TypeDef *port, uint8_t pin) : GpioDriver(port, pin) {
        Init(Mode::Input, OutType::OpenDrain, Pull::Up);
    }

    enum class Event {
        None, Pressed, Released, Held
    };

    inline Event tick() {
        const uint32_t now = GetMsTicks();
        const bool rawPressed = !Read();

        if (rawPressed != m_rawState) {
            m_rawState = rawPressed;
            m_lastChange = now;
        }

        Event e = Event::None;

        if (m_rawState != m_stableState) {
            if (hasElapsed(now, m_lastChange, DebounceMs)) {
                m_stableState = m_rawState;
                if (m_stableState) {
                    m_holdDeadline = now + HoldMs;
                    e = Event::Pressed;
                } else {
                    m_holdDeadline = 0;
                    e = Event::Released;
                }
            }
        } else if (m_stableState && HoldMs > 0 && m_holdDeadline != 0 &&
                   timeReached(now, m_holdDeadline)) {
            m_holdDeadline = now + HoldMs;
            e = Event::Held;
        }

        return e;
    }

private:
    static inline bool timeReached(uint32_t now, uint32_t deadline) {
        return static_cast<int32_t>(now - deadline) >= 0;
    }

    static inline bool hasElapsed(uint32_t now, uint32_t since, uint32_t duration) {
        return static_cast<uint32_t>(now - since) >= duration;
    }

    bool m_rawState = false;
    bool m_stableState = false;
    uint32_t m_lastChange = 0;
    uint32_t m_holdDeadline = 0;
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