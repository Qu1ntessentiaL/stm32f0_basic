#pragma once

#include <array>
#include <optional>

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
        size_t next = (head_ + 1) % MaxEvents;
        if (next == tail_) return false; // Очередь полна
        buffer_[head_] = ev;
        head_ = next;
        return true;
    }

    std::optional<Event> pop() {
        if (tail_ == head_) return std::nullopt; // Пуста
        Event ev = buffer_[tail_];
        tail_ = (tail_ + 1) % MaxEvents;
        return ev;
    }

private:
    std::array<Event, MaxEvents> buffer_{};
    size_t head_ = 0, tail_ = 0;
};