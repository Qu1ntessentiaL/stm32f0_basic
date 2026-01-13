#pragma once

#include <cstdint>
#include "config.h"
#include "TimDriver.hpp"
#include "RccDriver.hpp"

using namespace RccDriver;

class BeepManager {
public:
    explicit BeepManager(PwmDriver *driver)
            : m_driver(driver) {}

    void requestBeep(uint16_t freq = BEEP_FREQUENCY_HZ, uint16_t duration = BEEP_DURATION_MS) {
        m_freq = freq;
        m_active = true;

        // включаем звук
        m_driver->setPower(BEEP_POWER_PERCENT);   // duty
        m_driver->setFrequency(freq);

        m_endTime = GetMsTicks() + duration;
    }

    void poll() {
        if (!m_active) return;
        if (GetMsTicks() < m_endTime) return;

        m_driver->setPower(0);   // выключаем
        m_active = false;
    }

private:
    PwmDriver *m_driver;
    uint32_t m_endTime = 0;
    bool m_active = false;
    uint16_t m_freq = 0;
};