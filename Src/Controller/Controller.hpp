#pragma once

#include "EventQueue.hpp"
#include "SystemTime.h"
#include "PID.hpp"

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
    /**
     * @brief Логические режимы работы термостата.
     */
    enum class State : uint8_t {
        Idle,    ///< Цель достигнута, нагрев не требуется.
        Heating, ///< Нужно греть, загорается зелёный светодиод.
        Error    ///< Перегрев относительно цели, горит красный светодиод.
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
    State actionTemperatureSample(const Event &e);
    State actionDecreaseSetpoint(const Event &e);
    State actionIncreaseSetpoint(const Event &e);
    State actionPIDTick(const Event &);

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

    int m_setpoint = 250;                       ///< Уставка, задаваемая пользователем (в десятых долях °C, 250 = 25.0°C).
    int m_current = 0;                          ///< Текущая измеренная температура (в десятых долях °C).
    State m_state = State::Idle;                ///< Состояние автомата.
    bool m_showingSetpoint = false;             ///< Отображается ли сейчас уставка `t2`.
    uint32_t m_setpointDisplayDeadline = 0;     ///< Момент возврата к отображению `t1`.

    static constexpr uint32_t SetpointDisplayDurationMs = 3000; ///< Время показа `t2` после нажатия (мс).
    static constexpr int SetpointStep = 5;                      ///< Шаг изменения уставки (в десятых долях °C, 5 = 0.5°C).
    static constexpr int SetpointMin = -95;                     ///< Минимально допустимая уставка (в десятых долях °C, -95 = -9.5°C).
    static constexpr int SetpointMax = 995;                     ///< Максимально допустимая уставка (в десятых долях °C, 995 = 99.5°C).
    static constexpr int ErrorDelta = 30;                       ///< Перегрев относительно цели (в десятых долях °C, 30 = 3.0°C).

    /** PID-регулятор мощности нагрева (fixed-point integer) */

    /**
     * @brief PID-регулятор, управляющий мощностью нагревателя.
     *
     * Использует значения Kp, Ki, Kd в фиксированной точке (x1000),
     * что обеспечивает высокую скорость работы на MCU без FPU.
     */
    PIDInt m_pid = PIDInt(
            15000,      ///< Kp = 0.150
            0,        ///< Ki = 0.010
            0,        ///< Kd = 0.000
            0,     ///< Минимальная мощность
            1000   ///< Максимальная мощность (100% PWM)
    );

    /**
     * @brief Вычислить мощность нагревателя через PID.
     *
     * Вызывается только из updateOutputsFor() когда состояние != Error.
     */
    int computeHeatingPower() const;
};