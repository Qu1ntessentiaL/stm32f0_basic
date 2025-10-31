#pragma once

#include "tinyfsm.hpp"

// Определяем события
struct Start : tinyfsm::Event {
};
struct Done : tinyfsm::Event {
};
struct Fail : tinyfsm::Event {
};
struct Reset : tinyfsm::Event {
};

// Базовый класс FSM
class SensorFsm : public tinyfsm::Fsm<SensorFsm> {
public:
    virtual void react(Start const &) {}

    virtual void react(Done const &) {}

    virtual void react(Fail const &) {}

    virtual void react(Reset const &) {}

    void entry() {}

    void exit() {}
};

class Idle;

class Measure;

class Send;

class Error;