#pragma once

#include <cstdint>
#include "TimDriver.hpp"

class BeepManager {
public:
    explicit BeepManager(PwmDriver *driver)
            : m_driver(driver) {}

    void requestBeep(uint16_t freq = 3000, uint16_t duration = 50);

    void poll(); // Вызывать каждые 1 - 10 ms в tick

private:
    PwmDriver *m_driver;
    uint32_t m_endTime = 0;
    bool m_active = false;
    uint16_t m_freq = 0;
};