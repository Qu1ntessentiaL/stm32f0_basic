#pragma once

#include <cstdint>
#include <cstddef>

/**
 * @file config.h
 * @brief Глобальные конфигурационные константы проекта
 *
 * Этот файл содержит все магические числа, переведённые в именованные константы.
 * Позволяет централизованно управлять параметрами проекта.
 */

//=============================================================================
// HARDWARE CONFIGURATION
//=============================================================================

/// Системная частота (Hz)
static constexpr uint32_t SYSTEM_CLOCK_HZ = 48'000'000;

/// Частота PCLK1 для периферии (Hz)
static constexpr uint32_t PCLK1_HZ = 48'000'000;

/// I2C скорость (Hz)
static constexpr uint32_t I2C_SPEED_HZ = 100'000;

/// GPIO пины
namespace GPIO_PINS {
    static constexpr uint8_t USART_TX = 9;      ///< USART TX на PA9
    static constexpr uint8_t USART_RX = 10;     ///< USART RX на PA10
    static constexpr uint8_t RED_LED = 8;       ///< Red LED на PA8
    static constexpr uint8_t GREEN_LED = 6;     ///< Green LED на PA6
    static constexpr uint8_t BLUE_LED = 11;     ///< Blue LED на PA11
    static constexpr uint8_t LIGHT = 0;         ///< Light на PA0
    static constexpr uint8_t CHARGER = 15;      ///< Charger на PA15
}

//=============================================================================
// TIMER CONFIGURATION
//=============================================================================

/// TIM17 предделитель (для PWM частоты)
static constexpr uint32_t TIM17_PRESCALER = 47;

/// TIM17 период автоперезагрузки (ARR)
static constexpr uint32_t TIM17_ARR = 999;

/// Piezo (PWM звука) предделитель
static constexpr uint32_t PIEZO_PRESCALER = 47;

/// Piezo (PWM звука) период (ARR)
static constexpr uint32_t PIEZO_ARR = 499;

/// Heater (PWM нагрева) предделитель
static constexpr uint32_t HEATER_PRESCALER = 47;

/// Heater (PWM нагрева) период (ARR)
static constexpr uint32_t HEATER_ARR = 999;

//=============================================================================
// PWM/POWER CONFIGURATION
//=============================================================================

/// Максимальное значение мощности (0..PWM_MAX = 0..100%)
static constexpr uint16_t PWM_MAX = 1000;

/// Мощность звука (пиза) в % от max (0..1000)
static constexpr uint16_t BEEP_POWER_PERCENT = 500;

/// Частота звука по умолчанию (Hz)
static constexpr uint16_t BEEP_FREQUENCY_HZ = 3000;

/// Длительность звука по умолчанию (мс)
static constexpr uint16_t BEEP_DURATION_MS = 50;

//=============================================================================
// UART / EVENT LOOP CONFIGURATION
//=============================================================================

/// Длительность нажатия кнопки через UART (мс)
/// Когда пользователь отправляет символ '1'..'4' через UART,
/// генерируется нажатие (press) + отпускание (release) через это время
static constexpr uint32_t UART_BUTTON_PRESS_DURATION_MS = 50;

/// Размер очереди событий
static constexpr size_t EVENT_QUEUE_MAX_SIZE = 16;

/// Таймаут I2C операции (мс)
static constexpr uint8_t I2C_TIMEOUT_MS = 100;

//=============================================================================
// APP LOOP TIMING
//=============================================================================

/// Период тика 100ms (вызывается каждый 100-й вызов app_loop)
static constexpr uint8_t APP_LOOP_TICKS_PER_100MS = 100;

//=============================================================================
// TEMPERATURE CONTROLLER CONFIGURATION
//=============================================================================

/// Уставка по умолчанию (в десятых долях °C, 400 = 40.0°C)
static constexpr int CONTROLLER_SETPOINT_DEFAULT = 400;

/// Минимально допустимая уставка (в десятых долях °C, -95 = -9.5°C)
static constexpr int CONTROLLER_SETPOINT_MIN = -95;

/// Максимально допустимая уставка (в десятых долях °C, 995 = 99.5°C)
static constexpr int CONTROLLER_SETPOINT_MAX = 995;

/// Время показа уставки на дисплее после нажатия (мс)
static constexpr uint32_t CONTROLLER_SETPOINT_DISPLAY_DURATION_MS = 3000;

/// Перегрев, вызывающий error state (в десятых долях °C, 30 = 3.0°C)
static constexpr int CONTROLLER_ERROR_DELTA = 30;

/// Период дискретизации PID (мс)
static constexpr uint32_t CONTROLLER_PID_SAMPLE_PERIOD_MS = 1000;

/// Диапазон вывода PID контроллера (0..PWM_MAX)
static constexpr int32_t CONTROLLER_PID_OUT_MIN = 0;
static constexpr int32_t CONTROLLER_PID_OUT_MAX = 1000;

/// Диапазон интегрирующей части PID
static constexpr int32_t CONTROLLER_PID_INTEGR_MIN = -2000;
static constexpr int32_t CONTROLLER_PID_INTEGR_MAX = 2000;

/// PID коэффициенты (умножены на SCALE для fixed-point)
namespace CONTROLLER_PID {
    static constexpr int32_t KP = 40000;  ///< Пропорциональный коэффициент
    static constexpr int32_t KI = 500;    ///< Интегральный коэффициент
    static constexpr int32_t KD = 300;    ///< Дифференциальный коэффициент
    static constexpr int32_t SCALE = 1000; ///< Масштаб fixed-point (KP=40000 соответствует 40.0)
}

/// Ограничения температуры для отображения на дисплее
static constexpr int TEMPERATURE_DISPLAY_MIN = -99;  ///< Минимум -9.9°C
static constexpr int TEMPERATURE_DISPLAY_MAX = 999;  ///< Максимум 99.9°C
static constexpr int TEMPERATURE_DISPLAY_SCALE = 10; ///< Температура хранится в десятых долях

//=============================================================================
// BUTTON MANAGER CONFIGURATION
//=============================================================================

/// Интервал для двойного клика (мс)
static constexpr uint32_t BUTTONS_DOUBLE_CLICK_GAP_MS = 250;

/// Время для распознавания combo нажатия (мс)
static constexpr uint32_t BUTTONS_COMBO_LONG_THRESHOLD_MS = 400;

/// Время debounce для кнопок (мс)
static constexpr uint16_t BUTTONS_DEBOUNCE_MS = 30;

/// Время для распознавания hold события (мс)
static constexpr uint16_t BUTTONS_HOLD_MS = 100;

//=============================================================================
// DISPLAY CONFIGURATION
//=============================================================================

/// Размер текстового буфера на дисплее (символов)
static constexpr size_t DISPLAY_TEXT_BUFFER_SIZE = 5;

/// Диапазон двухзначного числа для дисплея
static constexpr int DISPLAY_TWO_DIGIT_MIN = 10;
static constexpr int DISPLAY_TWO_DIGIT_MAX = 99;

//=============================================================================
// SENSOR CONFIGURATION
//=============================================================================

/// Максимальное количество попыток читать датчик
static constexpr uint8_t SENSOR_MAX_RETRIES = 3;

//=============================================================================
// WATCHDOG CONFIGURATION
//=============================================================================

/// Период перезагрузки watchdog (мс), примерно 1 секунда
static constexpr uint32_t WATCHDOG_TIMEOUT_MS = 1000;

/// Делитель для IWDG
static constexpr uint8_t IWDG_PRESCALER = 6;  // PR=6 -> делитель=256

/// Значение для IWDG reload
static constexpr uint8_t IWDG_RELOAD_VALUE = 145; // ~1 сек: (145+1) * 256 / 37000
