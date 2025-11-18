#pragma once

#include "stm32f0xx.h"

class TimDriver {
public:
    using Callback = void (*)();

    explicit TimDriver(TIM_TypeDef *tim) : m_tim(tim), m_callback(nullptr) {}

    void Init(uint16_t prescaler, uint16_t autoReload) {
        enableClock();

        m_tim->PSC = prescaler;
        m_tim->ARR = autoReload;
        m_tim->CNT = 0;
        m_tim->DIER |= TIM_DIER_UIE;   // Update interrupt enable

        enableNVIC();
    }

    void setCallback(Callback cb) {
        m_callback = cb;
    }

    void handleIRQ() {
        if (m_tim->SR & TIM_SR_UIF) {
            m_tim->SR &= ~TIM_SR_UIF; // Clear update flag
            m_irqCount++;
            if (m_callback) m_callback();
        }
    }

    inline void Start() {
        m_tim->CR1 |= TIM_CR1_CEN;
    }

    inline void Stop() {
        m_tim->CR1 &= ~TIM_CR1_CEN;
    }

    inline uint16_t getIrqCount() const { return m_irqCount; }

    inline void clearIrqCount() { m_irqCount = 0; }

    inline void decIrqCount() {
        if (m_irqCount > 0) m_irqCount--;
    }

private:
    TIM_TypeDef *m_tim;
    Callback m_callback;
    volatile uint16_t m_irqCount = 0;

    void enableClock() {
        if (m_tim == TIM1) RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
        else if (m_tim == TIM3) RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
        else if (m_tim == TIM14) RCC->APB1ENR |= RCC_APB1ENR_TIM14EN;
        else if (m_tim == TIM16) RCC->APB2ENR |= RCC_APB2ENR_TIM16EN;
        else if (m_tim == TIM17) RCC->APB2ENR |= RCC_APB2ENR_TIM17EN;
    }

    void enableNVIC() {
        if (m_tim == TIM1) NVIC_EnableIRQ(TIM1_BRK_UP_TRG_COM_IRQn);
        else if (m_tim == TIM3) NVIC_EnableIRQ(TIM3_IRQn);
        else if (m_tim == TIM14) NVIC_EnableIRQ(TIM14_IRQn);
        else if (m_tim == TIM16) NVIC_EnableIRQ(TIM16_IRQn);
        else if (m_tim == TIM17) NVIC_EnableIRQ(TIM17_IRQn);
    }
};