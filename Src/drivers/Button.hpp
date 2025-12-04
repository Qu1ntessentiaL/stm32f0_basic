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

class ButtonsManager {
public:
    ButtonsManager(GPIO_TypeDef *portS1, uint8_t pinS1,
                   GPIO_TypeDef *portS2, uint8_t pinS2,
                   GPIO_TypeDef *portS3, uint8_t pinS3,
                   GPIO_TypeDef *portS4, uint8_t pinS4)
            : btnS1(portS1, pinS1),
              btnS2(portS2, pinS2),
              btnS3(portS3, pinS3),
              btnS4(portS4, pinS4) {}

    void poll(EventQueue &queue) {
        handle(btnS1, EventType::ButtonS1, lastRelease[0], queue);
        handle(btnS2, EventType::ButtonS2, lastRelease[1], queue);
        handle(btnS3, EventType::ButtonS3, lastRelease[2], queue);
        handle(btnS4, EventType::ButtonS4, lastRelease[3], queue);

        checkCombination(queue);
    }

private:
    static constexpr uint32_t DoubleClickGap = 250; // мс
    uint32_t lastRelease[4] = {0, 0, 0, 0};

    Button<> btnS1, btnS2, btnS3, btnS4;

    void handle(Button<> &b, EventType type, uint32_t &lastR, EventQueue &queue) {
        const auto now = RccDriver::GetMsTicks();
        auto e = b.tick();

        if (e == Button<>::Event::Pressed) {
            queue.push({type, 0});     // short press start
            lastR = 0;
        } else if (e == Button<>::Event::Held) {
            queue.push({type, 1});     // hold
        } else if (e == Button<>::Event::Released) {
            queue.push({type, 2});
            if (now - lastR <= DoubleClickGap) {
                queue.push({type, 3}); // double click code
            }
            lastR = now;
        }
    }

    void checkCombination(EventQueue &queue) {
        const bool s1 = !btnS1.Read();
        const bool s2 = !btnS2.Read();
        const uint32_t now = RccDriver::GetMsTicks();

        static uint32_t comboStart = 0;

        if (s1 && s2) {
            if (comboStart == 0) comboStart = now;

            if (now - comboStart > 400) { // long combo
                queue.push({EventType::ButtonS1, 10}); // combo long
                comboStart = 0;
            }
        } else {
            if (comboStart != 0 && (now - comboStart < 400)) {
                queue.push({EventType::ButtonS1, 9}); // short combo
            }
            comboStart = 0;
        }
    }
};