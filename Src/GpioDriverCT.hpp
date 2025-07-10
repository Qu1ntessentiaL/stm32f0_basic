#pragma once

#include "stm32f0xx.h"

enum class GpioPort {
    A, B, C
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
class GpioDriverCT {
    static_assert(Pin <= 15, "GPIO pin must be between 0 and 15");

    static constexpr uint32_t port_addr = GpioPortInfo<Port>::base_addr;
    static constexpr uint32_t pin_mask = (1U << Pin);
    static constexpr uint32_t pin_pos = Pin * 2;

    static constexpr GPIO_TypeDef *port() {
        return reinterpret_cast<GPIO_TypeDef *>(port_addr);
    }

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

    static inline void Init(Mode mode,
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

    static inline void SetAlternate(uint8_t af) {
        static_assert(!(Port == GpioPort::A && (Pin == 13 || Pin == 14)),
                      "Error: Cannot set alternate function on PA13 or PA14 (SWDIO/SWCLK)");

        EnableClock();

        const uint32_t index = Pin / 8;
        const uint32_t shift = (Pin % 8) * 4;
        port()->AFR[index] = (port()->AFR[index] & ~(0xF << shift)) | (af << shift);
    }

    static inline void Set() { port()->BSRR = pin_mask; }

    static inline void Reset() { port()->BRR = pin_mask; }

    static inline void Toggle() { port()->ODR ^= pin_mask; }

    static inline bool Read() { return (port()->IDR & pin_mask) != 0; }

private:
    static inline void EnableClock() {
        if (!(RCC->AHBENR & GpioPortInfo<Port>::clock)) {
            RCC->AHBENR |= GpioPortInfo<Port>::clock;
            __DSB();
        }
    }
};