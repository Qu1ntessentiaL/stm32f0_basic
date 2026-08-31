#pragma once

#include <cstdint>
#include "TimDriver.hpp"
#include "RccDriver.hpp"
#include "config.h"

/**
 * @brief Одна нота мелодии.
 * @note freq_hz == 0 обозначает паузу (тишину) длительностью duration_ms.
 */
struct Note {
    uint16_t freq_hz;
    uint16_t duration_ms;
};

/**
 * @brief Неблокирующий проигрыватель мелодий на пьезо-пищалке (PwmDriver).
 *
 * Работает по тому же принципу, что и BeepManager: play() запускает
 * воспроизведение, а poll() должен вызываться каждую итерацию app_loop()
 * и сам переключает ноты по времени (RccDriver::GetMsTicks()) — без
 * блокировки основного цикла.
 *
 * @note Массив notes не копируется, поэтому должен жить как минимум пока
 *       мелодия играет — на практике это всегда statically/constexpr таблица.
 */
class MelodyPlayer {
public:
    explicit MelodyPlayer(PwmDriver *driver) : m_driver(driver) {}

    /// Запустить (или перезапустить) воспроизведение мелодии.
    void play(const Note *notes, uint16_t count) {
        m_notes = notes;
        m_count = count;
        m_index = 0;
        m_active = (m_notes != nullptr && m_count > 0);
        if (m_active) startNote();
    }

    /// Немедленно прервать воспроизведение и выключить пищалку.
    void stop() {
        m_active = false;
        m_driver->setPower(0);
    }

    bool isPlaying() const { return m_active; }

    void poll() {
        if (!m_active) return;
        if (static_cast<int32_t>(RccDriver::GetMsTicks() - m_noteEndTime) < 0) return;

        if (++m_index >= m_count) {
            stop();
            return;
        }
        startNote();
    }

private:
    void startNote() {
        const Note &n = m_notes[m_index];
        if (n.freq_hz == 0) {
            m_driver->setPower(0);
        } else {
            m_driver->setFrequency(n.freq_hz);
            m_driver->setPower(BEEP_POWER_PERCENT);
        }
        m_noteEndTime = RccDriver::GetMsTicks() + n.duration_ms;
    }

    PwmDriver *m_driver;
    const Note *m_notes = nullptr;
    uint16_t m_count = 0;
    uint16_t m_index = 0;
    uint32_t m_noteEndTime = 0;
    bool m_active = false;
};
