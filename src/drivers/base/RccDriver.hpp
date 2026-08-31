#pragma once

#include "stm32f0xx.h"
#include "GpioDriver.hpp"

namespace RccDriver {
    /**
     *   Глобальный таймер миллисекунд — системная подсистема.
     *   Вынесено в System.hpp, чтобы AppContext не зависел
     *   от конкретной реализации SysTick.
     */
    extern volatile uint32_t g_msTicks;

    inline uint32_t GetMsTicks() {
        return g_msTicks;
    }

    /**
     * @brief Настроить тактирование на 48 МГц от внешнего кварцевого резонатора (HSE).
     *
     * Аппаратно: пассивный кварц 8 МГц + нагрузочные конденсаторы 20 пФ на
     * PF0 (OSC_IN) / PF1 (OSC_OUT). Это выделенные пины оскилятора на LQFP32
     * (STM32F030K6) — настраивать их через GpioDriver не нужно, они
     * переключаются автоматически при включении HSE (RCC_CR_HSEON).
     *
     * @note Жёсткая зависимость от HSE: если кварц не запустится (обрыв
     *       контакта, не распаян и т.п.), ожидание HSERDY ниже зависнет
     *       навечно — сознательно без fallback на HSI и без таймаута.
     */
    inline void InitMax48MHz() {
        // 1. Включить внешний кварц (HSE). HSEBYP=0 - пассивный резонатор,
        //    а не готовый генератор с одним активным выводом (bypass-режим).
        RCC->CR &= ~RCC_CR_HSEBYP;
        RCC->CR |= RCC_CR_HSEON;
        while (!(RCC->CR & RCC_CR_HSERDY)) {}

        // 2. Настроить PLL: HSE (8 МГц) / PREDIV(1) * 6 = 48 МГц
        RCC->CFGR2 &= ~RCC_CFGR2_PREDIV;
        RCC->CFGR2 |= RCC_CFGR2_PREDIV_DIV1;

        RCC->CFGR &= ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMUL);
        RCC->CFGR |= (RCC_CFGR_PLLSRC_HSE_PREDIV | RCC_CFGR_PLLMUL6);

        // 3. Включить PLL
        RCC->CR |= RCC_CR_PLLON;
        while (!(RCC->CR & RCC_CR_PLLRDY)) {}

        // 4. Выбрать PLL как системную частоту
        RCC->CFGR &= ~RCC_CFGR_SW;
        RCC->CFGR |= RCC_CFGR_SW_PLL;
        while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL) {}

        // 5. HSI больше не используется системным тактированием - выключаем,
        //    чтобы контроллер был полностью переведён на внешний кварц.
        RCC->CR &= ~RCC_CR_HSION;

        // 6. Обновить глобальную переменную SystemCoreClock
        SystemCoreClockUpdate();
    }

    inline void InitMCO() {
        GpioDriver mco(GPIOA, 8);
        mco.SetAlternateFunction(0);

        RCC->CFGR &= ~RCC_CFGR_MCO;
        RCC->CFGR |= RCC_CFGR_MCO_SYSCLK;
        RCC->CFGR |= RCC_CFGR_MCOPRE_DIV16;
    }

    inline void IWDG_Init() {
        // 1. Включить LSI (если еще не включен)
        RCC->CSR |= RCC_CSR_LSION;
        while ((RCC->CSR & RCC_CSR_LSIRDY) == 0) {
            // Ждем стабилизации генератора (~100 мс)
        }

        // 2. Разблокировать доступ к регистрами PR и RLR
        IWDG->KR = 0x5555;

        // 3. Настроить предделитель (PR)
        // PR = 6 -> делитель = 256
        IWDG->PR = 6;

        // 4. Настроить reload (RLR)
        // RLR = 145 -> ~1 сек (145 + 1) * 256 / 37000 ~ 1.01 с
        IWDG->RLR = 145;

        // 5. Обновить счётчик, чтобы сразу не сработал
        IWDG->KR = 0xAAAA;

        // 6. Запустить watchdog
        IWDG->KR = 0xCCCC;
    }

    inline void IWDG_Reload() {
        // Обновление watchdog
        IWDG->KR = 0xAAAA;
    }
} // namespace RccDriver