#include "my_fsm.hpp"

uint

// Имитация действий
static void start_measurement() { /* включаем АЦП */ }

static void send_data() { /* отправляем UART */ }

static void blink_error() { /* мигаем светодиодом */ }

// Idle
class Idle : public ButtonFsm {
    void react(ButtonPressed const &) override {
        start_measurement();
        transit<Measure>();
    }
};
FSM_INITIAL_STATE(ButtonFsm, Idle)

// Measure
class Measure : public ButtonFsm {
    void react(ButtonReleased const &) override {
        send_data();
        transit<Send>();
    }

    void react(TimerTick const &) override {
        blink_error();
        transit<Error>();
    }
};

// Send
class Send : public ButtonFsm {
    void react(ShortPress const &) override {
        transit<Idle>();
    }
};

// Error
class Error : public ButtonFsm {
    void react(ShortPress const &) override {
        transit<Idle>();
    }
};