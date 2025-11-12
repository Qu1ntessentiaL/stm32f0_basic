#pragma once

#include "EventQueue.hpp"
#include "SystemTime.h"

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
    };

    // Guard-функции и действия
    bool guardPress(const Event &e) const;
    State actionTemperatureSample(const Event &e);
    State actionDecreaseSetpoint(const Event &e);
    State actionIncreaseSetpoint(const Event &e);

    // Управление состоянием
    State evaluateState() const;
    void applyState(State newState);
    void updateOutputsFor(State state);

    // Работа с индикацией
    void displayCurrentTemperature();
    void displaySetpointTemperature();
    void displayTemperature(char label, float value);
    void ensureDisplayTimeout();

    // Вспомогательные функции
    static bool timeReached(uint32_t now, uint32_t deadline);

    /// Таблица переходов конечного автомата (определена в .cpp).
    static const Transition transitions[];

    float m_setpoint = 25.0f;                   ///< Уставка, задаваемая пользователем (°C).
    float m_current = 0.0f;                     ///< Текущая измеренная температура (°C).
    State m_state = State::Idle;                ///< Состояние автомата.
    bool m_showingSetpoint = false;             ///< Отображается ли сейчас уставка `t2`.
    uint32_t m_setpointDisplayDeadline = 0;     ///< Момент возврата к отображению `t1`.

    static constexpr uint32_t SetpointDisplayDurationMs = 3000; ///< Время показа `t2` после нажатия (мс).
    static constexpr float SetpointStep = 0.5f;                 ///< Шаг изменения уставки (°C).
    static constexpr float SetpointMin = -9.5f;                 ///< Минимально допустимая уставка (°C).
    static constexpr float SetpointMax = 99.5f;                 ///< Максимально допустимая уставка (°C).
    static constexpr float ErrorDelta = 3.0f;                   ///< Перегрев относительно цели (°C).
};