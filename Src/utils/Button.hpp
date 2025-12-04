#pragma once

#include "stm32f0xx.h"
#include "RccDriver.hpp"
#include "GpioDriver.hpp"
#include "Event.hpp"

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
        const uint32_t now = RccDriver::GetMsTicks();
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