#include "BeepManager.hpp"
#include "AppContext.hpp"

void BeepManager::requestBeep(uint16_t freq, uint16_t duration) {
    m_freq = freq;
    m_active = true;

    // включаем звук
    m_driver.setPower(500);   // duty
    m_driver.setFrequency(freq);

    m_endTime = GetMsTicks() + duration;
}

void BeepManager::poll() {
    if (!m_active) return;
    if (GetMsTicks() < m_endTime) return;

    m_driver.setPower(0);   // выключаем
    m_active = false;
}
