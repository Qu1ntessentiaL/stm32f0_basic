#include "my_fsm.hpp"

// Имитация действий
static void start_measurement() { /* включаем АЦП */ }

static void send_data() { /* отправляем UART */ }

static void blink_error() { /* мигаем светодиодом */ }

// ---- Idle ----
class Idle : public SensorFsm {
    void react(Start const &) override {
        start_measurement();
        transit<Measure>();
    }
};
FSM_INITIAL_STATE(SensorFsm, Idle)

// ---- Measure ----
class Measure : public SensorFsm {
    void react(Done const &) override {
        send_data();
        transit<Send>();
    }

    void react(Fail const &) override {
        blink_error();
        transit<Error>();
    }
};

// ---- Send ----
class Send : public SensorFsm {
    void react(Reset const &) override {
        transit<Idle>();
    }
};

// ---- Error ----
class Error : public SensorFsm {
    void react(Reset const &) override {
        transit<Idle>();
    }
};