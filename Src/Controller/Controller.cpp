#include "Controller.hpp"

#include "GpioDriver.hpp"
#include "ht1621.hpp"

#include <cstdio>

extern GpioDriver *red_led_ptr, *green_led_ptr;
extern HT1621B *disp_ptr;

namespace {
    /** Переводит значение температуры в десятые доли градуса с округлением. */
    inline int round_to_tenths(float value) {
        return static_cast<int>((value >= 0.0f)
                                ? (value * 10.0f + 0.5f)
                                : (value * 10.0f - 0.5f));
    }

    /** Возвращает абсолютный срок для таймаута (now + delta). */
    inline uint32_t make_deadline(uint32_t now, uint32_t delta) {
        return now + delta;
    }
}

const Controller::Transition Controller::transitions[] = {
        {EventType::TemperatureReady, Controller::State::Idle,    nullptr,                 &Controller::actionTemperatureSample},
        {EventType::TemperatureReady, Controller::State::Heating, nullptr,                 &Controller::actionTemperatureSample},
        {EventType::TemperatureReady, Controller::State::Error,   nullptr,                 &Controller::actionTemperatureSample},
        {EventType::ButtonS1,         Controller::State::Idle,    &Controller::guardPress, &Controller::actionDecreaseSetpoint},
        {EventType::ButtonS1,         Controller::State::Heating, &Controller::guardPress, &Controller::actionDecreaseSetpoint},
        {EventType::ButtonS1,         Controller::State::Error,   &Controller::guardPress, &Controller::actionDecreaseSetpoint},
        {EventType::ButtonS2,         Controller::State::Idle,    &Controller::guardPress, &Controller::actionIncreaseSetpoint},
        {EventType::ButtonS2,         Controller::State::Heating, &Controller::guardPress, &Controller::actionIncreaseSetpoint},
        {EventType::ButtonS2,         Controller::State::Error,   &Controller::guardPress, &Controller::actionIncreaseSetpoint},
};

/** Инициализация контроллера и синхронизация индикации. */
void Controller::init() {
    m_current = m_setpoint;
    m_showingSetpoint = false;
    m_setpointDisplayDeadline = 0;
    applyState(evaluateState());
    displayCurrentTemperature();
}

/** Обработка очередного события конечным автоматом. */
void Controller::processEvent(const Event &e) {
    for (const auto &transition: transitions) {
        if (transition.signal != e.type) continue;
        if (transition.source != m_state) continue;
        if (transition.guard && !(this->*transition.guard)(e)) continue;

        State next = (this->*transition.action)(e);
        applyState(next);
        return;
    }
}

/** Обработка тайм-аутов и фоновых задач. */
void Controller::poll() {
    ensureDisplayTimeout();
}

/** Guard: реагировать только на первое событие «нажата» (value == 0). */
bool Controller::guardPress(const Event &e) const {
    return e.value == 0.0f;
}

/** Action: сохранить новое измерение и перерассчитать состояние. */
Controller::State Controller::actionTemperatureSample(const Event &e) {
    m_current = e.value;

    if (!m_showingSetpoint) {
        displayCurrentTemperature();
    }

    return evaluateState();
}

/** Action: уменьшить уставку (ButtonS1). */
Controller::State Controller::actionDecreaseSetpoint(const Event &) {
    m_setpoint -= SetpointStep;
    if (m_setpoint < SetpointMin) m_setpoint = SetpointMin;

    m_showingSetpoint = true;
    m_setpointDisplayDeadline = make_deadline(GetMsTicks(), SetpointDisplayDurationMs);
    displaySetpointTemperature();

    return evaluateState();
}

/** Action: увеличить уставку (ButtonS2). */
Controller::State Controller::actionIncreaseSetpoint(const Event &) {
    m_setpoint += SetpointStep;
    if (m_setpoint > SetpointMax) m_setpoint = SetpointMax;

    m_showingSetpoint = true;
    m_setpointDisplayDeadline = make_deadline(GetMsTicks(), SetpointDisplayDurationMs);
    displaySetpointTemperature();

    return evaluateState();
}

/** Высчитать новое состояние автомата, исходя из текущих температур. */
Controller::State Controller::evaluateState() const {
    if (m_current > (m_setpoint + ErrorDelta)) {
        return State::Error;
    }
    if (m_current < m_setpoint) {
        return State::Heating;
    }
    return State::Idle;
}

/** Применить новое состояние и обновить индикаторы. */
void Controller::applyState(State newState) {
    m_state = newState;
    updateOutputsFor(newState);
}

/** Управление светодиодами в зависимости от состояния. */
void Controller::updateOutputsFor(State state) {
    const bool error = (state == State::Error);
    const bool heat = (state == State::Heating);

    if (green_led_ptr) {
        if (heat) green_led_ptr->Set();
        else green_led_ptr->Reset();
    }

    if (red_led_ptr) {
        if (error) red_led_ptr->Set();
        else red_led_ptr->Reset();
    }
}

/** Показать на индикаторе текущую температуру (`t1`). */
void Controller::displayCurrentTemperature() {
    m_showingSetpoint = false;
    displayTemperature('1', m_current);
}

/** Показать на индикаторе уставку (`t2`). */
void Controller::displaySetpointTemperature() {
    displayTemperature('2', m_setpoint);
}

/** Универсальный вывод значения на индикатор HT1621. */
void Controller::displayTemperature(char label, float value) {
    if (!disp_ptr) return;

    disp_ptr->ClearSegArea(false);

    float clamped = value;
    if (clamped < -9.9f) clamped = -9.9f;
    else if (clamped > 99.9f) clamped = 99.9f;

    int tenths = round_to_tenths(clamped);
    bool negative = tenths < 0;
    int magnitude = negative ? -tenths : tenths;
    if (magnitude > 999) magnitude = 999;

    int whole = magnitude / 10;
    int frac = magnitude % 10;

    char text[7] = {'t', label, ' ', ' ', ' ', '0', '\0'};

    if (negative) {
        text[3] = '-';
        if (whole < 10) {
            text[4] = static_cast<char>('0' + whole);
        } else {
            if (whole > 99) whole = 99;
            text[4] = static_cast<char>('0' + (whole / 10));
        }
    } else {
        char wholeBuf[4];
        std::snprintf(wholeBuf, sizeof(wholeBuf), "%2d", whole);
        text[3] = wholeBuf[0];
        text[4] = wholeBuf[1];
    }
    text[5] = static_cast<char>('0' + frac);

    disp_ptr->ShowString(text, false);
    disp_ptr->ShowDot(1, true, true);
}

/** Проверить, не истёк ли таймаут отображения уставки. */
void Controller::ensureDisplayTimeout() {
    if (!m_showingSetpoint) return;
    uint32_t now = GetMsTicks();
    if (timeReached(now, m_setpointDisplayDeadline)) {
        displayCurrentTemperature();
        applyState(evaluateState());
    }
}

/** Безопасное сравнение времени с учётом переполнения счётчика. */
bool Controller::timeReached(uint32_t now, uint32_t deadline) {
    return static_cast<int32_t>(now - deadline) >= 0;
}
