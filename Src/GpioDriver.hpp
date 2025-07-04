#pragma once

#include "stm32f0xx.h"

// Вспомогательная структура для преобразования GPIOx в базовый адрес
template<GPIO_TypeDef *Port>
struct GpioPortTraits;

// Специализации для каждого порта
template<>
struct GpioPortTraits<GPIOA> {
    static constexpr uint32_t ClockEnable = RCC_AHBENR_GPIOAEN;
};
template<>
struct GpioPortTraits<GPIOB> {
    static constexpr uint32_t ClockEnable = RCC_AHBENR_GPIOBEN;
};
template<>
struct GpioPortTraits<GPIOC> {
    static constexpr uint32_t ClockEnable = RCC_AHBENR_GPIOCEN;
};
// Добавьте другие порты по аналогии

/**
 * @brief Шаблонный класс для работы с GPIO через CMSIS
 * @tparam Port Указатель на структуру GPIO (GPIOA, GPIOB и т.д.)
 * @tparam Pin Номер пина (0-15)
 */
template<GPIO_TypeDef *Port, uint8_t Pin>
class GpioDriver {
    static_assert(Pin <= 15, "Invalid GPIO pin (0-15 allowed)");

    // Маска для работы с битами регистров
    constexpr static uint32_t PinMask = (1U << Pin);

    // Позиция в 2-битовых регистрах (MODER, PUPDR, OSPEEDR)
    constexpr static uint32_t PinPos = Pin * 2;

public:
    // Режимы GPIO
    enum class Mode {
        Input = 0b00,
        Output = 0b01,
        Alternate = 0b10,
        Analog = 0b11
    };

    enum class OutputType {
        PushPull = 0,
        OpenDrain = 1
    };

    enum class Pull {
        NoPull = 0b00,
        PullUp = 0b01,
        PullDown = 0b10
    };

    enum class Speed {
        Low = 0b00,
        Medium = 0b01,
        High = 0b10,
        VeryHigh = 0b11
    };

    // Инициализация GPIO
    static void Init(Mode mode,
                     OutputType otype = OutputType::PushPull,
                     Pull pull = Pull::NoPull,
                     Speed speed = Speed::Low) {
        EnableClock();

        // MODER: 2 бита на пин
        Port->MODER &= ~(0b11 << PinPos);
        Port->MODER |= (static_cast<uint32_t>(mode) << PinPos);

        // OTYPER: 1 бит на пин
        Port->OTYPER = (Port->OTYPER & ~PinMask) | ((otype == OutputType::OpenDrain) ? PinMask : 0);

        // OSPEEDR: 2 бита на пин
        Port->OSPEEDR &= ~(0b11 << PinPos);
        Port->OSPEEDR |= (static_cast<uint32_t>(speed) << PinPos);

        // PUPDR: 2 бита на пин
        Port->PUPDR &= ~(0b11 << PinPos);
        Port->PUPDR |= (static_cast<uint32_t>(pull) << PinPos);
    }

    // Установка пина в HIGH
    static void Set() {
        Port->BSRR = PinMask;
    }

    // Установка пина в LOW
    static void Reset() {
        Port->BRR = PinMask;
    }

    // Переключение состояния
    static void Toggle() {
        Port->ODR ^= PinMask;
    }

    // Чтение состояния пина
    static bool Read() {
        return (Port->IDR & PinMask) != 0;
    }

private:
    // Включение тактирования порта
    static void EnableClock() {
        RCC->AHBENR |= GpioPortTraits<Port>::ClockEnable;
    }
};