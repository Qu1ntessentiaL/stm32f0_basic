#pragma once

#include "stm32f0xx.h"

class GpioDriver {
    GPIO_TypeDef *m_port;
    uint8_t m_pin;
    uint32_t m_pin_mask;
    uint32_t m_pin_pos;

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
        Low = 0b00, Medium = 0b01, High = 0b10
    };

    constexpr GpioDriver(GPIO_TypeDef *port, uint8_t pin)
            : m_port(port), m_pin(pin),
              m_pin_mask(1U << pin),
              m_pin_pos(pin * 2) {}

    inline void Init(Mode mode,
                     OutType type = OutType::OpenDrain,
                     Pull pull = Pull::None,
                     Speed speed = Speed::Low) const {
        EnableClock();

        m_port->MODER = (m_port->MODER & ~(0b11 << m_pin_pos)) |
                        (static_cast<uint32_t>(mode) << m_pin_pos);
        m_port->OTYPER = (m_port->OTYPER & ~m_pin_mask) |
                         (static_cast<uint32_t>(type) << m_pin);
        m_port->PUPDR = (m_port->PUPDR & ~(0b11 << m_pin_pos)) |
                        (static_cast<uint32_t>(pull) << m_pin_pos);
        m_port->OSPEEDR = (m_port->OSPEEDR & ~(0b11 << m_pin_pos)) |
                          (static_cast<uint32_t>(speed) << m_pin_pos);
    }

    inline void Set() const { m_port->BSRR = m_pin_mask; }

    inline void Reset() const { m_port->BRR = m_pin_mask; }

    inline void Toggle() const { m_port->ODR ^= m_pin_mask; }

    inline bool Read() const { return (m_port->IDR & m_pin_mask) != 0; }

    inline void SetAlternateFunction(uint8_t af) const {
        EnableClock();

        m_port->MODER = (m_port->MODER & ~(0b11 << m_pin_pos)) | (0b10 << m_pin_pos);

        if (m_pin <= 7) {
            m_port->AFR[0] = (m_port->AFR[0] & ~(0xF << (m_pin * 4))) |
                             ((af & 0xF) << (m_pin * 4));
        } else {
            uint8_t offset = m_pin - 8;
            m_port->AFR[1] = (m_port->AFR[1] & ~(0xF << (offset * 4))) |
                             ((af & 0xF) << (offset * 4));
        }
    }

private:
    inline void EnableClock() const {
        if (m_port == GPIOA) RCC->AHBENR |= RCC_AHBENR_GPIOAEN;
        else if (m_port == GPIOB) RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
        else if (m_port == GPIOC) RCC->AHBENR |= RCC_AHBENR_GPIOCEN;
        else if (m_port == GPIOD) RCC->AHBENR |= RCC_AHBENR_GPIODEN;
        else if (m_port == GPIOF) RCC->AHBENR |= RCC_AHBENR_GPIOFEN;
        __DSB();
    }
};