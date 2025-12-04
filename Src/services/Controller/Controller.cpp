#include "Controller.hpp"

using namespace RccDriver;

namespace {
    /** Возвращает абсолютный срок для таймаута (now + delta). */
    inline uint32_t make_deadline(uint32_t now, uint32_t delta) {
        return now + delta;
    }
}

/**
 * @note В коде имеются два механизма wildcard (переход из любого состояния (.source) и
 *       переход по любому типу события (.signal) {что есть еще более сильный wildcard}).
 *       Поля таблицы с wildcard по состоянию должны располагаться ниже более конкретных
 *       (поля c Controller::State::Any ниже чем с Controller::State::Idle),
 *       а с wildcard по типу события в самом внизу (поля с EventType::Any).
 *       Иначе wildcard будут "перехватывать" все событие раньше, и реальные, специфичные переходы
 *       никогда не выполнятся.
 *
 *       !!! Это ключевой момент!
 */
const Controller::Transition Controller::transitions[] = {
        /// Реальные, специфичные переходы
        {EventType::Tick100ms,        Controller::State::Idle,    nullptr,                 &Controller::actionPIDTick,           Controller::State::Idle},
        {EventType::Tick100ms,        Controller::State::Heating, nullptr,                 &Controller::actionPIDTick,           Controller::State::Heating},

        /// Переходы, содержащие wildcard по состоянию
        // TemperatureReady: состояние вычисляется динамически через evaluateState()
        {EventType::TemperatureReady, Controller::State::Any,     nullptr,                 &Controller::actionTemperatureSample, Controller::ComputeState},
        // ButtonS1: уменьшение уставки, состояние вычисляется после изменения
        {EventType::ButtonS1,         Controller::State::Any,     &Controller::guardClickS1, &Controller::actionDecreaseSetpoint,  Controller::ComputeState},
        {EventType::ButtonS1,         Controller::State::Any,     &Controller::guardHeld,    &Controller::actionDecreaseSetpoint,  Controller::ComputeState},

        // ButtonS2: увеличение уставки, состояние вычисляется после изменения
        {EventType::ButtonS2,         Controller::State::Any,     &Controller::guardClickS2, &Controller::actionIncreaseSetpoint,  Controller::ComputeState},
        {EventType::ButtonS2,         Controller::State::Any,     &Controller::guardHeld,    &Controller::actionIncreaseSetpoint,  Controller::ComputeState},

        // Звук при нажатии на кнопки
        {EventType::ButtonS1,         Controller::State::Any,     nullptr,                   &Controller::actionBeep,              Controller::State::Any},
        {EventType::ButtonS2,         Controller::State::Any,     nullptr,                   &Controller::actionBeep,              Controller::State::Any},
        {EventType::ButtonS3,         Controller::State::Any,     nullptr,                   &Controller::actionBeep,              Controller::State::Any},
        {EventType::ButtonS4,         Controller::State::Any,     nullptr,                   &Controller::actionBeep,              Controller::State::Any},
        /// Переходы, содержащие wildcard по типу события
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
    // Обновляем флаги удержания для S1/S2
    if (e.type == EventType::ButtonS1) {
        if (e.value == 0) m_s1Held = false;      // Press
        else if (e.value == 1) m_s1Held = true;  // Held
    }

    if (e.type == EventType::ButtonS2) {
        if (e.value == 0) m_s2Held = false;
        else if (e.value == 1) m_s2Held = true;
    }

    for (const auto &transition: transitions) {
        if (transition.signal != e.type && transition.signal != EventType::Any) continue;
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

bool Controller::guardClickS1(const Event &e) const {
    return e.value == 2 && !m_s1Held;
}

bool Controller::guardClickS2(const Event &e) const {
    return e.value == 2 && !m_s2Held;
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

Controller::State Controller::actionBeep(const Event &e) {
    if (m_beep && e.value == 0) {
        m_beep->requestBeep(); // короткий пик
    }
    return m_state; // состояние не меняем
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
 * Работа светодиодов:
 * - Зеленый горит, если power > 0
 * - Красный горит в Error
 */
void Controller::updateOutputsFor(State state) {
    const bool error = (state == State::Error);

    int power = error ? 0 : m_heaterPower;
    if (power < 0) power = 0;
    if (power > 1000) power = 1000;

    // Управление нагревателем (PWM)
    if (m_heater) {
        m_heater->setPower(power);
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
    if (!m_display) return;

    m_display->ClearSegArea(false);

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

    m_display->ShowString(text, false);
    m_display->ShowDot(1, true, true);
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