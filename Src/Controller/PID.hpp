#pragma once
#include <cstdint>

/**
 * @brief Фиксированная-точность (integer-only) PID-регулятор.
 *
 * Класс предназначен для микроконтроллеров без FPU (например, STM32F0)
 * и реализует вычисление PID без использования float-типа.
 *
 * Внутренние коэффициенты Kp/Ki/Kd хранятся в виде целых чисел,
 * умноженных на SCALE (например, SCALE = 1000 -> Kp = 150 соответствует 0.150).
 *
 * Поддерживает:
 * - Ограничение выхода (outMin/outMax)
 * - Ограничение интегральной части (anti-windup)
 * - Пропуск производной на первом шаге
 * - Минимальную нагрузку на CPU (int32 + int64)
 */
class PIDInt {
public:
    /**
     * @brief Масштаб фиксированной точки.
     *
     * Все коэффициенты PID (Kp, Ki, Kd) должны быть заданы
     * умноженными на это значение.
     *
     * @code
     * Реальная величина:  Kp = 0.150
     * Значение в коде:    kp = 150 (0.150 * 1000)
     * @endcode
     */
    static constexpr int32_t SCALE = 1000;

    /**
     * @brief Конструктор PID-регулятора.
     *
     * @param kp Коэффициент пропорциональной части (в формате fixed-point * SCALE).
     * @param ki Коэффициент интегральной части (fixed-point * SCALE).
     * @param kd Коэффициент дифференциальной части (fixed-point * SCALE).
     * @param outMin Минимальное выходное значение (например, 0% мощности).
     * @param outMax Максимальное выходное значение (например, 1000 = 100% PWM).
     * @param integrMin Минимально допустимое значение интегратора (anti-windup).
     * @param integrMax Максимально допустимое значение интегратора.
     *
     * Все параметры — целые числа для высокой производительности.
     */
    PIDInt(int32_t kp, int32_t ki, int32_t kd,
           int32_t outMin = 0, int32_t outMax = 1000,
           int32_t integrMin = -50000, int32_t integrMax = 50000)
            : m_kp(kp), m_ki(ki), m_kd(kd),
              m_outMin(outMin), m_outMax(outMax),
              m_integrMin(integrMin), m_integrMax(integrMax)
    {}

    /**
     * @brief Сброс внутреннего состояния PID.
     *
     * Используется при старте системы, смене режима,
     * резкой смене уставки или после аварийной ситуации.
     *
     * Обнуляет интегральную часть и хранение предыдущей ошибки.
     */
    void reset() {
        m_integral = 0;
        m_prevError = 0;
        m_hasPrev = false;
    }

    /**
     * @brief Выполнить один шаг PID-расчёта.
     *
     * Вызывается при поступлении новых данных (например, после
     * каждого события TemperatureReady).
     *
     * @param setpoint	Целевое значение (например, температура ×10).
     * @param measured	Текущее измеренное значение (×10).
     * @return Управляющее воздействие, ограниченное диапазоном [outMin, outMax].
     *
     * Алгоритм:
     * - error = setpoint - measured
     * - Интегральная часть ограничивается (anti-windup)
     * - Производная рассчитывается только со второго шага
     * - Итоговый PID = (Kp*error + Ki*integral + Kd*derivative) / SCALE
     *
     * Используются только операции int32/int64,
     * что идеально подходит для Cortex-M0.
     */
    int32_t update(int32_t setpoint, int32_t measured)
    {
        int32_t error = setpoint - measured;

        // Интегральная часть
        m_integral += error;
        clamp(m_integral, m_integrMin, m_integrMax);

        // Производная часть
        int32_t derivative = 0;
        if (m_hasPrev) {
            derivative = error - m_prevError;
        } else {
            m_hasPrev = true; // пропуск деривации на первом шаге
        }
        m_prevError = error;

        // Основная формула PID
        // Все операции в int64 для предотвращения переполнения
        int64_t out =
                (int64_t)m_kp * error +
                (int64_t)m_ki * m_integral +
                (int64_t)m_kd * derivative;

        // Деление один раз -> максимум производительности
        out /= SCALE;

        // Ограничение выхода
        clamp(out, (int64_t)m_outMin, (int64_t)m_outMax);

        return (int32_t)out;
    }

private:
    // Коэффициенты PID в формате fixed-point
    int32_t m_kp;	///< Пропорциональная часть (×SCALE)
    int32_t m_ki;	///< Интегральная часть (×SCALE)
    int32_t m_kd;	///< Производная часть (×SCALE)

    // ===== Ограничения =====
    int32_t m_outMin;	///< Минимальное значение выхода
    int32_t m_outMax;	///< Максимальное значение выхода
    int32_t m_integrMin; ///< Минимум для интегратора
    int32_t m_integrMax; ///< Максимум для интегратора

    // Внутренние переменные
    int32_t m_integral = 0;     ///< Интегральная сумма ошибки
    int32_t m_prevError = 0;    ///< Ошибка прошлого шага
    bool    m_hasPrev = false;  ///< Признак наличия предыдущей ошибки

    /**
     * @brief Универсальная функция ограничения значения.
     */
    template<typename T>
    static void clamp(T &v, T lo, T hi) {
        if (v < lo) v = lo;
        if (v > hi) v = hi;
    }
};