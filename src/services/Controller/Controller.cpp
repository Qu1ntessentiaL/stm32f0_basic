#include "Controller.hpp"
#include "config.h"
#include "ds18x20.hpp"

using namespace RccDriver;

namespace {
    /** Возвращает абсолютный срок для таймаута (now + delta). */
    inline uint32_t make_deadline(uint32_t now, uint32_t delta) {
        return now + delta;
    }
}

/**
 * @note Порядок строк в таблице важен.
 *
 * processEvent() берёт ПЕРВЫЙ подходящий переход и сразу выходит.
 * «Подходит» значит: совпал тип события (или signal == EventType::Any)
 * и текущее состояние (или source == State::Any), плюс прошёл guard.
 *
 * Поэтому более узкие правила должны стоять выше широких:
 *   1) конкретное событие + конкретное состояние  (Tick100ms + Idle)
 *   2) конкретное событие + State::Any            (кнопки из любого состояния)
 *   3) EventType::Any                             (самый широкий — только в конце)
 *
 * Если поставить State::Any / EventType::Any выше узкого правила,
 * широкое правило перехватит событие, а нужный переход никогда не сработает.
 */
const Controller::Transition Controller::transitions[] = {
        /// Реальные, специфичные переходы
        {EventType::Tick100ms,        Controller::State::Idle,    nullptr,                 &Controller::actionPIDTick,           Controller::State::Idle},
        {EventType::Tick100ms,        Controller::State::Heating, nullptr,                 &Controller::actionPIDTick,           Controller::State::Heating},
        {EventType::Tick100ms,        Controller::State::Error,   nullptr,                 &Controller::actionErrorTick,         Controller::State::Error},

        /// Меню (оверлей): выше уставки, термосостояние не меняем
        {EventType::ButtonS4,         Controller::State::Any,     &Controller::guardMenuS4,        &Controller::actionMenuToggle, Controller::ComputeState},
        {EventType::ButtonS1,         Controller::State::Any,     &Controller::guardMenuComboExit, &Controller::actionMenuToggle, Controller::ComputeState},
        {EventType::ButtonS1,         Controller::State::Any,     &Controller::guardMenuPressS1,   &Controller::actionMenuPrev,   Controller::ComputeState},
        {EventType::ButtonS2,         Controller::State::Any,     &Controller::guardMenuPressS2,   &Controller::actionMenuNext,   Controller::ComputeState},
        {EventType::ButtonS3,         Controller::State::Any,     &Controller::guardMenuPressS3,   &Controller::actionMenuApply,  Controller::ComputeState},

        /// Переходы, содержащие wildcard по состоянию
        {EventType::TemperatureReady, Controller::State::Any,     nullptr,                   &Controller::actionTemperatureSample, Controller::ComputeState},
        {EventType::ButtonS1,         Controller::State::Any,     &Controller::guardClickS1, &Controller::actionDecreaseSetpoint,  Controller::ComputeState},
        {EventType::ButtonS1,         Controller::State::Any,     &Controller::guardHeld,    &Controller::actionDecreaseSetpoint,  Controller::ComputeState},

        {EventType::ButtonS2,         Controller::State::Any,     &Controller::guardClickS2, &Controller::actionIncreaseSetpoint,  Controller::ComputeState},
        {EventType::ButtonS2,         Controller::State::Any,     &Controller::guardHeld,    &Controller::actionIncreaseSetpoint,  Controller::ComputeState},

        {EventType::ButtonS1,         Controller::State::Any,     nullptr,                   &Controller::actionBeep,              Controller::ComputeState},
        {EventType::ButtonS2,         Controller::State::Any,     nullptr,                   &Controller::actionBeep,              Controller::ComputeState},
        {EventType::ButtonS3,         Controller::State::Any,     nullptr,                   &Controller::actionBeep,              Controller::ComputeState},
        {EventType::ButtonS4,         Controller::State::Any,     nullptr,                   &Controller::actionBeep,              Controller::ComputeState},
};

/** Инициализация контроллера и синхронизация индикации. */
void Controller::init() {
    m_have_sample = false;
    m_sensor_fault = false;
    m_current = 0;
    m_showingSetpoint = false;
    m_setpointDisplayDeadline = 0;
    m_pid.reset();
    m_heaterPower = 0;
    m_lastPidTimestamp = GetMsTicks();
    m_in_menu = false;
    m_menu_item = 0;
    applyLight();
    applyState(State::Idle);
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

    if (e.type == EventType::ButtonS3) {
        if (e.value == 0) m_s3Held = false;
        else if (e.value == 1) m_s3Held = true;
    }

    if (e.type == EventType::ButtonS4) {
        if (e.value == 0) m_s4Held = false;
        else if (e.value == 1) m_s4Held = true;
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
    ensureMenuTimeout();
    ensureDisplayTimeout();
}

/** Guard: реагировать только на первое событие «нажата» (value == 0). */
bool Controller::guardPress(const Event &e) const {
    return e.value == 0;
}

/** Guard: реагировать на повторные события «кнопка нажата» (value == 1). */
bool Controller::guardHeld(const Event &e) const {
    return !m_in_menu && e.value == 1;
}

bool Controller::guardClickS1(const Event &e) const {
    return guardNotInMenu(e) && e.value == 2 && !m_s1Held;
}

bool Controller::guardClickS2(const Event &e) const {
    return guardNotInMenu(e) && e.value == 2 && !m_s2Held;
}

bool Controller::guardMenuS4(const Event &e) const {
    return e.value == 0 && !menuInputLocked();
}

bool Controller::guardNotInMenu(const Event &) const {
    return !m_in_menu;
}

bool Controller::guardMenuPressS1(const Event &e) const {
    return m_in_menu && e.value == 0 && !menuInputLocked();
}

bool Controller::guardMenuPressS2(const Event &e) const {
    return m_in_menu && e.value == 0 && !menuInputLocked();
}

bool Controller::guardMenuPressS3(const Event &e) const {
    return m_in_menu && e.value == 0 && !menuInputLocked();
}

bool Controller::guardMenuComboExit(const Event &e) const {
    return m_in_menu && e.value == 9 && !menuInputLocked();
}

bool Controller::menuInputLocked() const {
    return !timeReached(GetMsTicks(), m_menu_lock_until);
}

void Controller::armMenuLock() {
    m_menu_lock_until = GetMsTicks() + MenuInputLockMs;
}

void Controller::maybeBeep() {
    if (m_key_sound && m_beep) {
        m_beep->requestBeep();
    }
}

/** Action: сохранить новое измерение и перерассчитать состояние. */
Controller::State Controller::actionTemperatureSample(const Event &e) {
    if (e.slot != DS18X20_CONTROL_SLOT) {
        return m_state;
    }

    if (DS18X20::is_error(static_cast<int16_t>(e.value))) {
        m_heaterPower = 0;
        m_have_sample = false;
        m_sensor_fault = true;
        if (!m_in_menu && !m_showingSetpoint) {
            displaySensorError();
        }
        return State::Error;
    }

    m_have_sample = true;
    m_sensor_fault = false;
    m_current = e.value;

    if (!m_in_menu && !m_showingSetpoint) {
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

Controller::State Controller::actionErrorTick(const Event &) {
    if (m_red_led) {
        if (++m_error_blink >= 5) {
            m_error_blink = 0;
            m_red_led->Toggle();
        }
    }
    return m_state;
}

Controller::State Controller::actionBeep(const Event &e) {
    if (m_key_sound && m_beep && e.value == 0) {
        m_beep->requestBeep();
    }
    return m_state;
}

Controller::State Controller::actionMenuToggle(const Event &) {
    armMenuLock();
    if (m_in_menu) {
        leaveMenu();
    } else {
        enterMenu();
    }
    return m_state;
}

Controller::State Controller::actionMenuPrev(const Event &) {
    if (m_menu_item == 0) {
        m_menu_item = static_cast<uint8_t>(MenuItemCount - 1);
    } else {
        --m_menu_item;
    }
    armMenuLock();
    maybeBeep();
    touchMenuDeadline();
    displayMenu();
    return m_state;
}

Controller::State Controller::actionMenuNext(const Event &) {
    ++m_menu_item;
    if (m_menu_item >= MenuItemCount) {
        m_menu_item = 0;
    }
    armMenuLock();
    maybeBeep();
    touchMenuDeadline();
    displayMenu();
    return m_state;
}

Controller::State Controller::actionMenuApply(const Event &) {
    if (m_menu_item == 0) {
        m_backlight = !m_backlight;
        applyLight();
    } else {
        m_key_sound = !m_key_sound;
    }
    armMenuLock();
    maybeBeep();
    touchMenuDeadline();
    displayMenu();
    return m_state;
}

bool Controller::isDouble(const Event &e) {
    return e.value == 3;
}

bool Controller::isComboShort(const Event &e) {
    return e.value == 9;
}

/** Высчитать новое состояние автомата, исходя из текущих температур. */
Controller::State Controller::evaluateState() const {
    if (m_sensor_fault) {
        return State::Error;
    }
    if (!m_have_sample) {
        return State::Idle;
    }
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
        if (previous != State::Error) {
            m_error_blink = 0;
            if (m_red_led) {
                m_red_led->Set();
            }
        }
    } else if (previous == State::Error) {
        m_lastPidTimestamp = GetMsTicks();
        m_error_blink = 0;
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
 * - Красный мигает в Error
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

    if (m_red_led && !error) {
        m_red_led->Reset();
    }
}

/** Показать на индикаторе текущую температуру (`t1`). */
void Controller::displayCurrentTemperature() {
    m_showingSetpoint = false;
    if (m_sensor_fault) {
        displaySensorError();
        return;
    }
    if (!m_have_sample) {
        if (m_display) {
            m_display->ShowString("t1 --");
            m_display->ShowDot(1, false);
            m_display->Flush();
        }
        return;
    }
    displayTemperature('1', m_current);
}

void Controller::displaySensorError() {
    if (!m_display) return;
    m_display->ShowString("t1 Err");
    m_display->ShowDot(1, false);
    m_display->Flush();
}

/** Показать на индикаторе уставку (`tt`). */
void Controller::displaySetpointTemperature() {
    displayTemperature('t', m_setpoint);
}

/** Универсальный вывод значения на индикатор HT1621. */
void Controller::displayTemperature(char label, int value) {
    if (!m_display) return;

    m_display->ClearSegArea();

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

    m_display->ShowString(text);
    m_display->ShowDot(1, true);
    m_display->Flush();
}

void Controller::displayMenu() {
    if (!m_display) return;
    const char *name = (m_menu_item == 0) ? "LIGht" : "SoUnd";
    m_display->ShowString(name);
    m_display->ShowDot(1, m_menu_item == 0 ? m_backlight : m_key_sound);
    m_display->Flush();
}

void Controller::enterMenu() {
    m_in_menu = true;
    m_menu_item = 0;
    m_showingSetpoint = false;
    maybeBeep();
    touchMenuDeadline();
    displayMenu();
}

void Controller::leaveMenu() {
    m_in_menu = false;
    maybeBeep();
    displayCurrentTemperature();
}

void Controller::applyLight() {
    if (!m_light) return;
    if (m_backlight) {
        m_light->Set();
    } else {
        m_light->Reset();
    }
}

void Controller::touchMenuDeadline() {
    m_menu_deadline = make_deadline(GetMsTicks(), MenuIdleTimeoutMs);
}

void Controller::ensureMenuTimeout() {
    if (!m_in_menu) return;
    if (timeReached(GetMsTicks(), m_menu_deadline)) {
        leaveMenu();
    }
}

/** Проверить, не истёк ли таймаут отображения уставки. */
void Controller::ensureDisplayTimeout() {
    if (m_in_menu || !m_showingSetpoint) return;
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
    // Если управление нагревателем выключено в config.h, PID не считаем вовсе:
    // ветка ниже отбрасывается компилятором, а m_heater у нас в любом случае nullptr.
    if constexpr (!HEATER_CONTROL_ENABLED) {
        return 0;
    }
    return m_pid.update(m_setpoint, m_current, dtMs);
}