#pragma once

#include "stm32f0xx.h"
#include "GpioDriver.hpp"

namespace RccDriver {
    inline void InitMax48MHz() {
        // 1. Включить HSI (должен быть включен по умолчанию, но гарантируем)
        RCC->CR |= RCC_CR_HSION;
        while (!(RCC->CR & RCC_CR_HSIRDY)) {}

        // 2. Настроить PLL: HSI / 2 * 12 = 48 МГц
        RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMUL);
        RCC->CFGR |= (RCC_CFGR_PLLSRC_HSI_DIV2 | RCC_CFGR_PLLMUL12);

        // 3. Включить PLL
        RCC->CR |= RCC_CR_PLLON;
        while (!(RCC->CR & RCC_CR_PLLRDY)) {}

        // 4. Выбрать PLL как системную частоту
        RCC->CFGR &= ~RCC_CFGR_SW;
        RCC->CFGR |= RCC_CFGR_SW_PLL;
        while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}

        // 5. Обновить глобальную переменную SystemCoreClock
        SystemCoreClockUpdate();
    }

    inline void InitMCO() {
        GpioDriver mco(GPIOA, 8);
        mco.SetAlternateFunction(0);

        RCC->CFGR &= ~RCC_CFGR_MCO;
        RCC->CFGR |= RCC_CFGR_MCO_SYSCLK;
        RCC->CFGR |= RCC_CFGR_MCOPRE_DIV16;
    }

    /**
     * @brief SysTick initialization
     * @param periodUs
     * @param sysClockHz
     */
    inline void InitSysTickUs(uint32_t periodUs, uint32_t sysClockHz) {
        // Кол-во тактов на заданный период (в микросекундах)
        uint32_t ticks = (sysClockHz / 1'000'000) * periodUs;

        if (ticks > SysTick_LOAD_RELOAD_Msk) {
            // Значение превышает максимум (24 бита)
            // Можно вернуть ошибку или обрезать
            ticks = SysTick_LOAD_RELOAD_Msk;
        }

        SysTick->LOAD = ticks - 1;
        SysTick->VAL = 0; // Сброс счётчика

        SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk | // Использовать HCLK
                        SysTick_CTRL_TICKINT_Msk |   // Включить прерывание
                        SysTick_CTRL_ENABLE_Msk;     // Запустить таймер
    }
}