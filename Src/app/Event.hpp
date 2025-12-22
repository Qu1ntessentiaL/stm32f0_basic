#pragma once

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
    TemperatureReady, ///< Fresh temperature sample is available (value holds Celsius degrees).
    Tick100ms,        ///< Legacy periodic event (unused).
    DisplayTimeout,   ///< Request to finish displaying the setpoint and revert to current temperature.

    Any               ///< Для перехода по любому событию (wildcard)
};

struct Event {
    EventType type;
    int value; // Универсальное поле (температура в десятых долях градуса, например)
};

class EventQueue {
public:
    static constexpr size_t MaxEvents = 16;

    bool push(const Event &ev) {
        if (m_queue.full()) {
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

private:
    etl::circular_buffer<Event, MaxEvents> m_queue;
};
