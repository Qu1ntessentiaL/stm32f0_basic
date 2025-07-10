#pragma once

#include "stm32f0xx.h"
#include "GpioDriverCT.hpp"

enum class UsartInstance {
    COM1
};

template<UsartInstance Usart>
struct UsartInstanceInfo;

template<>
struct UsartInstanceInfo<UsartInstance::COM1> {
    enum : uint32_t {
        base_addr = USART1_BASE,
        clock = RCC_APB2ENR_USART1EN,
        alt_func = 1 // AF1 для PA9/PA10
    };

    static inline USART_TypeDef *USART() {
        return reinterpret_cast<USART_TypeDef *>(base_addr);
    }
};

template<UsartInstance Usart,
        GpioPort PortRx, uint8_t PinRx,
        GpioPort PortTx, uint8_t PinTx,
        uint32_t Baudrate = 115200>
class UsartDriver {
    using m_gpio_rx = GpioDriverCT<PortRx, PinRx>;
    using m_gpio_tx = GpioDriverCT<PortTx, PinTx>;
    using m_info = UsartInstanceInfo<Usart>;

    static inline USART_TypeDef *USART() { return m_info::USART(); }

    static inline void EnableClock() {
        if (!(RCC->APB2ENR & m_info::clock)) {
            RCC->APB2ENR |= m_info::clock;
            __DSB();
        }
    }

public:
    static inline void Init(uint32_t apb_clk_hz) {
        m_gpio_rx::Init(m_gpio_rx ::Mode::Alternate);
        m_gpio_rx::SetAlternate(m_info::alt_func);

        m_gpio_tx::Init(m_gpio_tx::Mode::Alternate);
        m_gpio_tx::SetAlternate(m_info::alt_func);

        EnableClock();

        RCC->CFGR3 &= ~RCC_CFGR3_USART1SW;
        RCC->CFGR3 |= RCC_CFGR3_USART1SW_0; // SYSCLK

        USART()->BRR = (apb_clk_hz + Baudrate / 2) / Baudrate;
        USART()->CR1 = USART_CR1_RE | USART_CR1_TE;
        USART()->CR2 = 0;
        USART()->CR1 |= USART_CR1_UE; // Enable USART

        //USART()->CR1 |= USART_CR1_RXNEIE;
        //NVIC_EnableIRQ(USART1_IRQn);
    }

    static inline void SendByte(uint8_t data) {
        while (!(USART()->ISR & USART_ISR_TXE));
        USART1->TDR = data;
    }

    static inline void SendString(const char *str) {
        while (*str)
            SendByte(static_cast<uint8_t>(*str++));
    }

    static inline void SendU16(uint16_t value) {
        SendByte(static_cast<uint8_t>(value & 0xFF));
        SendByte(static_cast<uint8_t>((value >> 8) & 0xFF));
    }
};
