#pragma once

#define APBCLK   48000000UL
#define BAUDRATE 115200UL

class UartDriver {

public:
    static inline void Init() {
        SET_BIT(RCC->AHBENR, RCC_AHBENR_GPIOAEN);
        SET_BIT (RCC->APB2ENR, RCC_APB2ENR_USART1EN);

        SET_BIT (GPIOA->MODER, GPIO_MODER_MODER9_1);                //PA9 Tx
        CLEAR_BIT (GPIOA->OTYPER, GPIO_OTYPER_OT_9);
        SET_BIT (GPIOA->OSPEEDR, GPIO_OSPEEDR_OSPEEDR9);
        GPIOA->AFR[1] |= 0x0110;

        CLEAR_BIT (GPIOA->MODER, GPIO_MODER_MODER10);                //PA10 Rx

        RCC->CFGR3 &= ~RCC_CFGR3_USART1SW;
        RCC->CFGR3 |= RCC_CFGR3_USART1SW_0;                    //System clock (SYSCLK) selected as USART1 clock

        USART1->BRR = (APBCLK + BAUDRATE / 2) / BAUDRATE;            //BaudRate = 115200
        USART1->CR1 &= ~USART_CR1_M;                                //Data = 8 bit
        USART1->CR2 &= ~USART_CR2_STOP;                             //1 стоп-бит
        USART1->CR1 |= USART_CR1_TE;
        USART1->CR1 |= USART_CR1_RE;
        USART1->CR1 |= USART_CR1_UE;                                //Enable Transmitter, Receiver and USART

        //USART1->CR1 |= USART_CR1_RXNEIE;
        //NVIC_EnableIRQ (USART1_IRQn);
    }

    void SendByte(char data) {
        while (!(USART1->ISR & USART_ISR_TC));
        USART1->TDR = data;
    }

    void SendString(char *str) {
        uint8_t i = 0;
        while (str[i])
            SendByte(str[i++]);
    }

    void SendU16(uint16_t num) {
        SendByte((uint8_t) ((num & 0x00ff)));
        SendByte((uint8_t) ((num & 0xff00) >> 8));
    }
};

/*
extern "C" void USART1_IRQHandler(void) {
    if (USART1->SR & USART_CR1_RXNEIE) {
        USART1->SR &= ~USART_CR1_RXNEIE;
    }
}
*/
