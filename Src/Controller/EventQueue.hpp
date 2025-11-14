#pragma once

#include <array>
#include <optional>
#include <cstdint>

/**
 * @brief All high-level events that can be handled by the controller.
 */
enum class EventType : uint8_t {
    None,               ///< Placeholder event, carries no semantic meaning.
    ButtonS1,           ///< User interacted with button S1 (value encodes press/hold/release).
    ButtonS2,           ///< User interacted with button S2 (value encodes press/hold/release).
    ButtonS3,           ///< Reserved button event.
    ButtonS4,           ///< Reserved button event.
    TemperatureReady,   ///< Fresh temperature sample is available (value holds Celsius degrees).
    Tick100ms,          ///< Legacy periodic event (unused).
    DisplayTimeout,     ///< Request to finish displaying the setpoint and revert to current temperature.
};

struct Event {
    EventType type;
    int value;     // Универсальное поле (температура в десятых долях градуса, например)
};

class EventQueue {
public:
    static constexpr size_t MaxEvents = 16;

    bool push(const Event &ev) {
        size_t next = (m_head + 1) % MaxEvents;
        if (next == m_tail) return false; // Очередь полна
        m_buffer[m_head] = ev;
        m_head = next;
        return true;
    }

    std::optional<Event> pop() {
        if (m_tail == m_head) return std::nullopt; // Пуста
        Event ev = m_buffer[m_tail];
        m_tail = (m_tail + 1) % MaxEvents;
        return ev;
    }

private:
    std::array<Event, MaxEvents> m_buffer{};
    size_t m_head = 0,
            m_tail = 0;
};