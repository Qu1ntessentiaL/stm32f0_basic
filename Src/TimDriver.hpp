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

/**
 * @brief Драйвер для PWM на STM32F0.
 *
 * Настраивает:
 * - GPIO в режим AF
 * - Таймер в режим PWM (channel 1)
 * - PSC/ARR формируют частоту
 * - CCR1 = скважность (0…ARR)
 */
class PwmDriver {
public:
    explicit PwmDriver(TIM_TypeDef *tim, uint8_t channel)
            : m_tim(tim), m_channel(channel) {}

    /**
     * @brief Инициализация PWM
     * @param prescaler делитель таймера
     * @param autoReload период (ARR)
     */
    void Init(uint16_t prescaler, uint16_t autoReload) {
        enableClock();

        m_tim->PSC = prescaler;
        m_tim->ARR = autoReload;

        // PWM mode 1
        if (m_channel == 1) {
            m_tim->CCMR1 |= (6 << TIM_CCMR1_OC1M_Pos); // PWM mode 1
            m_tim->CCMR1 |= TIM_CCMR1_OC1PE;           // preload enable
            m_tim->CCER |= TIM_CCER_CC1E;
        }

        m_tim->CR1 |= TIM_CR1_ARPE;  // авто-перезагрузка preload
        m_tim->EGR |= TIM_EGR_UG;    // обновить shadow-регистры

        setInverted(true);
        Start();
    }

    /**
     * @brief Установить мощность (0..1000)
     * @param value
     */
    void setPower(int value) {
        if (value < 0) value = 0;
        if (value > 1000) value = 1000;

        uint32_t ccr = (m_tim->ARR * value) / 1000;

        if (m_channel == 1)
            m_tim->CCR1 = ccr;
    }

    void setInverted(bool inverted) {
        if (m_channel == 1) {
            if (inverted)
                m_tim->CCER |= TIM_CCER_CC1P;
            else
                m_tim->CCER &= ~TIM_CCER_CC1P;
        }
    }

    inline void Start() { m_tim->CR1 |= TIM_CR1_CEN; }

    inline void Stop() { m_tim->CR1 &= ~TIM_CR1_CEN; }

    void setFrequency(uint32_t frequency) {
        if (frequency == 0) {
            m_tim->CCR1 = 0;
            return;
        }

        uint32_t timClk = 48000000;
        uint32_t period = timClk / frequency;

        uint16_t psc = period / 65535 + 1;
        uint16_t arr = period / psc;

        m_tim->PSC = psc - 1;
        m_tim->ARR = arr - 1;
        m_tim->CCR1 = arr / 2;
    }

private:
    TIM_TypeDef *m_tim;
    uint8_t m_channel;

    void enableClock() {
        if (m_tim == TIM1) RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
        if (m_tim == TIM3) RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;
        if (m_tim == TIM14) RCC->APB1ENR |= RCC_APB1ENR_TIM14EN;
        if (m_tim == TIM16) RCC->APB2ENR |= RCC_APB2ENR_TIM16EN;
        if (m_tim == TIM17) RCC->APB2ENR |= RCC_APB2ENR_TIM17EN;
    }
};