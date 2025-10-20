#pragma once

#include <array>
#include <optional>
#include <cstdint>

enum class EventType : uint8_t {
    None,
    ButtonS1,
    ButtonS2,
    ButtonS3,
    ButtonS4,
    TemperatureReady,
    Tick100ms,
};

struct Event {
    EventType type;
    float value;   // Универсальное поле (температура, например)
};

class EventQueue {
public:
    static constexpr size_t MaxEvents = 16;

    bool push(const Event &ev) {
        size_t next = (m_head + 1) % MaxEvents;
        if (next == m_tail) return false; // Очередь полна
        buffer_[m_head] = ev;
        m_head = next;
        return true;
    }

    std::optional<Event> pop() {
        if (m_tail == m_head) return std::nullopt; // Пуста
        Event ev = buffer_[m_tail];
        m_tail = (m_tail + 1) % MaxEvents;
        return ev;
    }

private:
    std::array<Event, MaxEvents> buffer_{};
    size_t m_head = 0,
            m_tail = 0;
};