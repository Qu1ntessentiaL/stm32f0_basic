#pragma once

#include <cstdint>
#include "uart.hpp"

// #define PRINT_PID

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
           int32_t integrMin = -5000, int32_t integrMax = 5000,
           uint32_t sampleTimeMs = 1000, int32_t deadband = 0)
            : m_kp(kp), m_ki(ki), m_kd(kd),
              m_outMin(outMin), m_outMax(outMax),
              m_integrMin(integrMin), m_integrMax(integrMax),
              m_sampleTimeMs(sampleTimeMs ? sampleTimeMs : 1U),
              m_deadband(deadband >= 0 ? deadband : 0) {}

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
     * @param setpoint_x10	Целевое значение (например, температура x10).
     * @param measured_x10	Текущее измеренное значение (x10).
     * @return Управляющее воздействие, ограниченное диапазоном [outMin, outMax].
     *
     * Алгоритм:
     * - error = setpoint - measured
     * - Интегральная часть ограничивается (anti-windup)
     * - Производная рассчитывается только со второго шага
     * - Итоговый PID = (Kp * error + Ki * integral + Kd * derivative) / SCALE
     *
     * Используются только операции int32/int64,
     * что идеально подходит для Cortex-M0.
     */
    int32_t update(int32_t setpoint_x10, int32_t measured_x10, uint32_t dt_ms) {
        if (dt_ms == 0) {
            dt_ms = m_sampleTimeMs;
        }
        if (dt_ms == 0) {
            dt_ms = 1;
        }
        int32_t error = setpoint_x10 - measured_x10;

        if (m_deadband > 0 && abs32(error) <= m_deadband) {
            error = 0;
        }

        // Интегральная часть
        int32_t baseDt = static_cast<int32_t>(m_sampleTimeMs ? m_sampleTimeMs : 1U);
        int64_t integralIncrement = (int64_t) error * (int64_t) dt_ms;
        integralIncrement /= baseDt;
        m_integral += static_cast<int32_t>(integralIncrement);
        clamp(m_integral, m_integrMin, m_integrMax);

        // Производная часть
        int32_t derivative = 0;
        if (m_hasPrev) {
            int64_t delta = (int64_t) (error - m_prevError) * baseDt;
            derivative = static_cast<int32_t>(delta / (int64_t) dt_ms);
        } else {
            m_hasPrev = true;
        }
        m_prevError = error;

        // P, I, D термы отдельно (чтобы можно было печатать)
        int64_t p_term = (int64_t) m_kp * error;
        int64_t i_term = (int64_t) m_ki * m_integral;
        int64_t d_term = (int64_t) m_kd * derivative;

        // Сумма до деления -> "сырое" значение
        int64_t sum = p_term + i_term + d_term;

        // Масштабируем выход
        int64_t out = sum / SCALE;

        // Ограничиваем диапазон
        clamp(out, (int64_t) m_outMin, (int64_t) m_outMax);

#if defined PRINT_PID
        uart_write_str("err=");
        uart_write_int(error);

        uart_write_str(" P=");
        uart_write_int((int32_t) (p_term / SCALE));

        uart_write_str(" I=");
        uart_write_int((int32_t) (i_term / SCALE));

        uart_write_str(" D=");
        uart_write_int((int32_t) (d_term / SCALE));

        uart_write_str(" raw=");
        uart_write_int((int32_t) (sum / SCALE));

        uart_write_str(" out=");
        uart_write_int((int32_t) out);

        uart_write_str("\r\n");
#endif

        return (int32_t) out;
    }

    void setSampleTimeMs(uint32_t sampleTimeMs) {
        if (sampleTimeMs == 0) sampleTimeMs = 1;
        m_sampleTimeMs = sampleTimeMs;
    }

    void setDeadband(int32_t deadband_x10) {
        if (deadband_x10 < 0) deadband_x10 = 0;
        m_deadband = deadband_x10;
    }

private:
    // Коэффициенты PID в формате fixed-point
    int32_t m_kp;    ///< Пропорциональная часть (×SCALE)
    int32_t m_ki;    ///< Интегральная часть (×SCALE)
    int32_t m_kd;    ///< Производная часть (×SCALE)

    // Ограничения
    int32_t m_outMin;    ///< Минимальное значение выхода (коэффициент заполнения ШИМ)
    int32_t m_outMax;    ///< Максимальное значение выхода (коэффициент заполнения ШИМ)
    int32_t m_integrMin; ///< Минимум для интегратора
    int32_t m_integrMax; ///< Максимум для интегратора

    // Внутренние переменные
    int32_t m_integral = 0;     ///< Интегральная сумма ошибки
    int32_t m_prevError = 0;    ///< Ошибка прошлого шага
    bool m_hasPrev = false;  ///< Признак наличия предыдущей ошибки
    uint32_t m_sampleTimeMs;    ///< Номинальный интервал дискретизации (мс)
    int32_t m_deadband;         ///< Мёртвая зона по ошибке (в десятых градуса)

    /**
     * @brief Универсальная функция ограничения значения.
     * @tparam T
     * @param v
     * @param lo
     * @param hi
     */
    template<typename T>
    static void clamp(T &v, T lo, T hi) {
        if (v < lo) v = lo;
        if (v > hi) v = hi;
    }

    static int32_t abs32(int32_t v) {
        return v < 0 ? -v : v;
    }
};