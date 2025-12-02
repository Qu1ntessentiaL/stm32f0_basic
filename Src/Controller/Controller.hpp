#pragma once

#include "EventQueue.hpp"
#include "SystemTime.h"
#include "PID.hpp"
#include "BeepManager.hpp"

/**
 * @brief Высокоуровневый термостат с табличным конечным автоматом.
 *
 * Контроллер реагирует на нажатия кнопок и новые измерения температуры.
 * Поведение описывается через таблицу переходов: каждая строка содержит
 * исходное состояние, событие, необязательный guard и действие.
 * Такой стиль делает расширение логики максимально наглядным.
 */
class Controller {
public:
    Controller(BeepManager *beep) : m_beep(beep) {}

    /**
     * @brief Логические режимы работы термостата.
     */
    enum class State : uint8_t {
        Idle,    ///< Цель достигнута, нагрев не требуется.
        Heating, ///< Нужно греть, загорается зелёный светодиод.
        Error,   ///< Перегрев относительно цели, горит красный светодиод.

        Any      ///< Для перехода из любого состояния (wildcard)
    };

    /**
     * @brief Специальное значение для поля `to` в таблице переходов.
     *
     * Означает, что целевое состояние нужно вычислить динамически через action-функцию.
     * Используется для переходов, где состояние зависит от текущих значений температуры.
     */
    static constexpr State ComputeState = static_cast<State>(0xFF);

    /**
     * @brief Инициализировать внутренние структуры и обновить индикацию.
     */
    void init();

    /**
     * @brief Передать событие в конечный автомат.
     *
     * @param e Событие от кнопок или датчика.
     */
    void processEvent(const Event &e);

    /**
     * @brief Периодическая обработка фоновых задач (таймауты и пр.).
     */
    void poll();

private:
    using Guard = bool (Controller::*)(const Event &) const;
    using Action = State (Controller::*)(const Event &);

    /**
     * @brief Строка таблицы переходов.
     */
    struct Transition {
        EventType signal;  ///< Какой тип события обрабатываем.
        State source;      ///< В каком состоянии переход допустим.
        Guard guard;       ///< Дополнительная проверка (nullptr — без условий).
        Action action;     ///< Выполняемое действие, возвращает новое состояние.
        State to;          ///< Целевое состояние (ComputeState означает "вычислить через action").
    };

    // Guard-функции и действия
    bool guardPress(const Event &e) const;
    bool guardHeld(const Event &e) const;
    State actionTemperatureSample(const Event &e);
    State actionDecreaseSetpoint(const Event &e);
    State actionIncreaseSetpoint(const Event &e);
    State actionPIDTick(const Event &e);
    State actionBeep(const Event &e);
    bool guardClickS1(const Event &e) const;
    bool guardClickS2(const Event &e) const;
    bool isDouble(const Event &e);
    bool isComboShort(const Event &e);

    // Управление состоянием
    State evaluateState() const;
    void applyState(State newState);
    void updateOutputsFor(State state);

    // Работа с индикацией
    void displayCurrentTemperature();
    void displaySetpointTemperature();
    void displayTemperature(char label, int value);  // value в десятых долях градуса
    void ensureDisplayTimeout();

    // Вспомогательные функции
    static bool timeReached(uint32_t now, uint32_t deadline);

    /// Таблица переходов конечного автомата (определена в .cpp).
    static const Transition transitions[];

    int m_setpoint = 400;                       ///< Уставка, задаваемая пользователем (в десятых долях °C, 250 = 25.0°C).
    int m_current = 0;                          ///< Текущая измеренная температура (в десятых долях °C).
    State m_state = State::Idle;                ///< Состояние автомата.
    bool m_showingSetpoint = false;             ///< Отображается ли сейчас уставка `t2`.
    uint32_t m_setpointDisplayDeadline = 0;     ///< Момент возврата к отображению `t1`.
    int m_heaterPower = 0;                      ///< Последнее вычисленное значение мощности (0..1000).
    uint32_t m_lastPidTimestamp = 0;            ///< Время последнего обновления PID (мс).

    static constexpr uint32_t SetpointDisplayDurationMs = 3000; ///< Время показа `t2` после нажатия (мс).
    static constexpr int SetpointStep = 5;                      ///< Шаг изменения уставки (в десятых долях °C, 5 = 0.5°C).
    static constexpr int SetpointMin = -95;                     ///< Минимально допустимая уставка (в десятых долях °C, -95 = -9.5°C).
    static constexpr int SetpointMax = 995;                     ///< Максимально допустимая уставка (в десятых долях °C, 995 = 99.5°C).
    static constexpr int ErrorDelta = 30;                       ///< Перегрев относительно цели (в десятых долях °C, 30 = 3.0°C).
    static constexpr uint32_t PidNominalSamplePeriodMs = 1000;  ///< Базовый период дискретизации PID.
    static constexpr int PidDeadband = 1;                       ///< Мёртвая зона PID (0.2°C).

    bool m_s1Held = false, m_s2Held = false;

    /** PID-регулятор мощности нагрева (fixed-point integer) */

    /**
     * @brief PID-регулятор, управляющий мощностью нагревателя.
     *
     * Использует значения Kp, Ki, Kd в фиксированной точке (x1000),
     * что обеспечивает высокую скорость работы на MCU без FPU.
     */
    PIDInt m_pid = PIDInt(
            40000,       ///< Kp
            500,         ///< Ki
            300,           ///< Kd
            0,           ///< Минимальная мощность
            1000,        ///< Максимальная мощность (100% PWM)
            -2000,
            2000,
            PidNominalSamplePeriodMs,
            PidDeadband
    );

    /**
     * @brief Вычислить мощность нагревателя через PID.
     *
     * Вызывается только из updateOutputsFor() когда состояние != Error.
     */
    int computeHeatingPower(uint32_t dtMs);

    BeepManager *m_beep;
};