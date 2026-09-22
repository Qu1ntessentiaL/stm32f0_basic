#pragma once

#include "config.h"
#include <etl/circular_buffer.h>
#include <etl/optional.h>
#include <cstdint>

/**
 * @brief All high-level events that can be handled by the controller.
 */
enum class EventType : uint8_t {
    None,             ///< Placeholder event, carries no semantic meaning.
    ButtonS1,         ///< User interacted with button S1 (value encodes press/hold/release).
    ButtonS2,         ///< User interacted with button S2 (value encodes press/hold/release).
    ButtonS3,         ///< Reserved button event.
    ButtonS4,         ///< Reserved button event.
    TemperatureReady, ///< Новое измерение: value — десятые °C, slot — номер датчика.
    Tick100ms,        ///< Legacy periodic event (unused).
    DisplayTimeout,   ///< Request to finish displaying the setpoint and revert to current temperature.

    Any               ///< Для перехода по любому событию (wildcard)
};

struct Event {
    EventType type;
    int value;          ///< Температура в десятых °C, код кнопки и т.п.
    uint8_t slot = 0;   ///< Номер датчика для TemperatureReady; для остальных событий 0.
};

class EventQueue {
public:
    static constexpr size_t MaxEvents = EVENT_QUEUE_MAX_SIZE;

    bool push(const Event &ev) {
        if (m_queue.full()) {
            ++m_dropped;
            return false;
        }
        m_queue.push(ev);
        return true;
    }

    etl::optional<Event> pop() {
        if (m_queue.empty()) {
            return etl::nullopt;
        }
        Event ev = m_queue.front();
        m_queue.pop();
        return ev;
    }

    uint32_t dropped_count() const { return m_dropped; }

private:
    etl::circular_buffer<Event, MaxEvents> m_queue;
    uint32_t m_dropped = 0;
};
