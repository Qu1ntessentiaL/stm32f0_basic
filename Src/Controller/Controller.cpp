#include "Controller.hpp"

#include "GpioDriver.hpp"
#include "ht1621.hpp"
#include "TimDriver.hpp"

extern GpioDriver *red_led_ptr;
extern HT1621B *disp_ptr;
extern PwmDriver *heater_ptr;

namespace {
    /** Возвращает абсолютный срок для таймаута (now + delta). */
    inline uint32_t make_deadline(uint32_t now, uint32_t delta) {
        return now + delta;
    }
}

/**
 * @note Строки с .source == Controller::State::Any должны стоять после более конкретных,
 *       иначе wildcard "перехватит" событие раньше.
 *       Т.е. правильный порядок:
 *          1) Специфичные переходы (Idle, Heating, Error)
 *          2) Универсальные (Any)
 */
const Controller::Transition Controller::transitions[] = {
        // TemperatureReady: состояние вычисляется динамически через evaluateState()
        {EventType::TemperatureReady, Controller::State::Idle,    nullptr,                 &Controller::actionTemperatureSample, Controller::ComputeState},
        {EventType::TemperatureReady, Controller::State::Heating, nullptr,                 &Controller::actionTemperatureSample, Controller::ComputeState},
        {EventType::TemperatureReady, Controller::State::Error,   nullptr,                 &Controller::actionTemperatureSample, Controller::ComputeState},

        // ButtonS1: уменьшение уставки, состояние вычисляется после изменения
        {EventType::ButtonS1,         Controller::State::Any,     &Controller::guardPress, &Controller::actionDecreaseSetpoint,  Controller::ComputeState},
        {EventType::ButtonS1,         Controller::State::Any,     &Controller::guardHeld,  &Controller::actionDecreaseSetpoint,  Controller::ComputeState},

        // ButtonS2: увеличение уставки, состояние вычисляется после изменения
        {EventType::ButtonS2,         Controller::State::Any,     &Controller::guardPress, &Controller::actionIncreaseSetpoint,  Controller::ComputeState},
        {EventType::ButtonS2,         Controller::State::Any,     &Controller::guardHeld,  &Controller::actionIncreaseSetpoint,  Controller::ComputeState},

        {EventType::Tick100ms,        Controller::State::Idle,    nullptr,                 &Controller::actionPIDTick,           Controller::State::Idle},
        {EventType::Tick100ms,        Controller::State::Heating, nullptr,                 &Controller::actionPIDTick,           Controller::State::Heating},
};

/** Инициализация контроллера и синхронизация индикации. */
void Controller::init() {
    m_current = m_setpoint;
    m_showingSetpoint = false;
    m_setpointDisplayDeadline = 0;
    m_pid.reset();
    m_heaterPower = 0;
    m_lastPidTimestamp = GetMsTicks();
    applyState(evaluateState());
    displayCurrentTemperature();
}

/** Обработка очередного события конечным автоматом. */
void Controller::processEvent(const Event &e) {
    for (const auto &transition: transitions) {
        if (transition.signal != e.type) continue;
        if (transition.source != m_state && transition.source != State::Any) continue;
        if (transition.guard && !(this->*transition.guard)(e)) continue;

        // Определяем целевое состояние
        State next;
        if (transition.to == ComputeState) {
            // Вычисляем через action-функцию (для TemperatureReady и кнопок)
            next = (this->*transition.action)(e);
        } else {
            // Используем явно указанное состояние из таблицы
            next = transition.to;
            // Вызываем action для побочных эффектов (если есть)
            if (transition.action) {
                (this->*transition.action)(e);
            }
        }
        
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
    return e.value == 0;
}

/** Guard: реагировать на повторные события «кнопка нажата» (value == 1). */
bool Controller::guardHeld(const Event &e) const {
    return e.value == 1;
}

/** Action: сохранить новое измерение и перерассчитать состояние. */
Controller::State Controller::actionTemperatureSample(const Event &e) {
    m_current = e.value;

    if (!m_showingSetpoint) {
        displayCurrentTemperature();
    }

    State next = evaluateState();

    if (next != State::Error) {
        uint32_t now = GetMsTicks();
        if (m_state == State::Error) {
            m_pid.reset();
            m_heaterPower = 0;
            m_lastPidTimestamp = now;
        }
        uint32_t dt = now - m_lastPidTimestamp;
        if (dt == 0) {
            dt = 1;
        }
        m_lastPidTimestamp = now;
        m_heaterPower = computeHeatingPower(dt);
    } else {
        m_heaterPower = 0;
    }

    return next;
}

/** Action: уменьшить уставку (ButtonS1). */
Controller::State Controller::actionDecreaseSetpoint(const Event &) {
    m_setpoint -= SetpointStep;
    if (m_setpoint < SetpointMin) m_setpoint = SetpointMin;

    m_showingSetpoint = true;
    m_setpointDisplayDeadline = make_deadline(GetMsTicks(), SetpointDisplayDurationMs);
    displaySetpointTemperature();
    m_pid.reset();
    m_heaterPower = 0;
    m_lastPidTimestamp = GetMsTicks();

    return evaluateState();
}

/** Action: увеличить уставку (ButtonS2). */
Controller::State Controller::actionIncreaseSetpoint(const Event &) {
    m_setpoint += SetpointStep;
    if (m_setpoint > SetpointMax) m_setpoint = SetpointMax;

    m_showingSetpoint = true;
    m_setpointDisplayDeadline = make_deadline(GetMsTicks(), SetpointDisplayDurationMs);
    displaySetpointTemperature();
    m_pid.reset();
    m_heaterPower = 0;
    m_lastPidTimestamp = GetMsTicks();

    return evaluateState();
}

Controller::State Controller::actionPIDTick(const Event &) {
    // Периодически переустанавливаем выходы, чтобы учесть PWM/индикацию.
    updateOutputsFor(m_state);
    return m_state; // Состояние не изменяем
}

bool Controller::isDouble(const Event &e) {
    return e.value == 3;
}

bool Controller::isComboShort(const Event &e) {
    return e.value == 9;
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
    State previous = m_state;
    m_state = newState;

    if (newState == State::Error) {
        m_heaterPower = 0;
        m_pid.reset();
    } else if (previous == State::Error) {
        m_lastPidTimestamp = GetMsTicks();
    }

    updateOutputsFor(newState);
}

/**
 * @brief Применить новое состояние автомата и обновить выходы.
 *
 * FSM остаётся, но мощность нагревателя вычисляется PID-регулятором.
 *
 * - State::Error -> мощность = 0
 * - State::Heating/Idle -> PID выдаёт 0..1000
 *
 * Светодиоды работают как раньше:
 * - Зеленый горит, если power > 0
 * - Красный горит в Error
 */
void Controller::updateOutputsFor(State state) {
    const bool error = (state == State::Error);

    int power = error ? 0 : m_heaterPower;
    if (power < 0) power = 0;
    if (power > 1000) power = 1000;

    // Управление нагревателем (PWM)
    if (heater_ptr) {
        heater_ptr->setPower(power);
        // setPower ожидает значение от 0 до 1000 (0..100%)
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
void Controller::displayTemperature(char label, int value) {
    if (!disp_ptr) return;

    disp_ptr->ClearSegArea(false);

    // value уже в десятых долях градуса, ограничиваем диапазон
    int clamped = value;
    if (clamped < -99) clamped = -99;  // -9.9°C
    else if (clamped > 999) clamped = 999;  // 99.9°C

    bool negative = clamped < 0;
    int magnitude = negative ? -clamped : clamped;

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
        // Форматируем whole как двузначное число с ведущим нулём (без snprintf)
        if (whole > 99) whole = 99;
        text[3] = static_cast<char>('0' + (whole / 10));
        text[4] = static_cast<char>('0' + (whole % 10));
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

/**
 * @brief Вычислить мощность нагрева через PID (0..1000).
 *
 * Текущая температура и уставка уже хранятся в полях класса
 * m_current и m_setpoint, поэтому функция просто делегирует
 * расчёт PID-регулятору.
 */
int Controller::computeHeatingPower(uint32_t dtMs) {
    return m_pid.update(m_setpoint, m_current, dtMs);
}