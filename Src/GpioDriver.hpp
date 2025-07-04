#pragma once

#include "stm32f0xx.h"

enum class GpioPort {
    A, B, C, D, F
};

template<GpioPort Port>
struct GpioPortInfo;

template<>
struct GpioPortInfo<GpioPort::A> {
    enum : uint32_t {
        base_addr = GPIOA_BASE,
        clock = RCC_AHBENR_GPIOAEN
    };
};

template<>
struct GpioPortInfo<GpioPort::B> {
    enum : uint32_t {
        base_addr = GPIOB_BASE,
        clock = RCC_AHBENR_GPIOBEN
    };
};

template<>
struct GpioPortInfo<GpioPort::C> {
    enum : uint32_t {
        base_addr = GPIOC_BASE,
        clock = RCC_AHBENR_GPIOCEN
    };
};

template<GpioPort Port, uint8_t Pin>
class GpioDriver {
    static_assert(Pin <= 15, "GPIO pin must be between 0 and 15");

    enum : uint32_t {
        port_addr = GpioPortInfo<Port>::base_addr
    };

    static GPIO_TypeDef *port() {
        return reinterpret_cast<GPIO_TypeDef *>(port_addr);
    }

    enum : uint32_t {
        pin_mask = (1U << Pin),
        pin_pos = Pin * 2
    };

public:
    enum class Mode {
        Input = 0b00, Output = 0b01, Alternate = 0b10, Analog = 0b11
    };
    enum class OutType {
        PushPull = 0, OpenDrain = 1
    };
    enum class Pull {
        None = 0b00, Up = 0b01, Down = 0b10
    };
    enum class Speed {
        Low, Medium, High, VeryHigh
    };

    static void Init(Mode mode,
                            OutType type = OutType::PushPull,
                            Pull pull = Pull::None,
                            Speed speed = Speed::Low) {
        EnableClock();

        port()->MODER &= ~(0b11 << pin_pos);
        port()->MODER |= static_cast<uint32_t>(mode) << pin_pos;

        port()->OTYPER = (port()->OTYPER & ~pin_mask) |
                         (static_cast<uint32_t>(type) << Pin);

        port()->PUPDR &= ~(0b11 << pin_pos);
        port()->PUPDR |= static_cast<uint32_t>(pull) << pin_pos;

        port()->OSPEEDR &= ~(0b11 << pin_pos);
        port()->OSPEEDR |= static_cast<uint32_t>(speed) << pin_pos;
    }

    static void Set() { port()->BSRR = pin_mask; }

    static void Reset() { port()->BRR = pin_mask; }

    static void Toggle() { port()->ODR ^= pin_mask; }

    static bool Read() { return (port()->IDR & pin_mask) != 0; }

private:
    static void EnableClock() {
        RCC->AHBENR |= GpioPortInfo<Port>::clock;
        __DSB();
    }
};