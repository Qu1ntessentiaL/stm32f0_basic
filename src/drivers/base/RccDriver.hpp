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
     * Источник SYSCLK после @ref InitMax48MHz.
     * HSI 8 МГц — только если не завёлся ни HSE, ни PLL: UART и 1-Wire поедут.
     */
    enum class Sysclk : uint8_t {
        HsePll48, ///< HSE 8 МГц × 6
        HsiPll48, ///< HSI/2 × 12, кварц не стартовал
        Hsi8      ///< PLL тоже не готов
    };

    inline Sysclk g_sysclk = Sysclk::Hsi8;

    inline Sysclk ClockSource() { return g_sysclk; }

    namespace detail {
        /** Спин до флага. На HSI 8 МГц ~200000 итераций ≈ 100…150 мс. */
        inline bool wait_mask(volatile uint32_t &reg, uint32_t mask, uint32_t spins) {
            while (spins--) {
                if ((reg & mask) == mask) return true;
            }
            return false;
        }

        inline bool wait_eq(volatile uint32_t &reg, uint32_t mask, uint32_t value, uint32_t spins) {
            while (spins--) {
                if ((reg & mask) == value) return true;
            }
            return false;
        }

        constexpr uint32_t kOscSpins = 200000;

        inline void disable_pll() {
            RCC->CR &= ~RCC_CR_PLLON;
            wait_eq(RCC->CR, RCC_CR_PLLRDY, 0, kOscSpins);
        }

        inline bool switch_to_pll() {
            FLASH->ACR |= FLASH_ACR_LATENCY | FLASH_ACR_PRFTBE;
            RCC->CR |= RCC_CR_PLLON;
            if (!wait_mask(RCC->CR, RCC_CR_PLLRDY, kOscSpins)) {
                disable_pll();
                return false;
            }
            RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
            return wait_eq(RCC->CFGR, RCC_CFGR_SWS, RCC_CFGR_SWS_PLL, kOscSpins);
        }
    }

    /**
     * @brief 48 МГц: HSE 8 МГц × 6, при срыве кварца — HSI/2 × 12.
     *
     * Кварц: PF0/PF1, HSEBYP=0. SysTick ещё нет — таймаут спином по HSI.
     * 1 wait-state flash обязателен выше 24 МГц.
     */
    inline void InitMax48MHz() {
        g_sysclk = Sysclk::Hsi8;

        RCC->CR &= ~RCC_CR_HSEBYP;
        RCC->CR |= RCC_CR_HSEON;
        const bool hse_ok = detail::wait_mask(RCC->CR, RCC_CR_HSERDY, detail::kOscSpins);

        if (hse_ok) {
            RCC->CFGR2 = (RCC->CFGR2 & ~RCC_CFGR2_PREDIV) | RCC_CFGR2_PREDIV_DIV1;
            RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMUL)) |
                        RCC_CFGR_PLLSRC_HSE_PREDIV | RCC_CFGR_PLLMUL6;
            if (detail::switch_to_pll()) {
                RCC->CR &= ~RCC_CR_HSION;
                g_sysclk = Sysclk::HsePll48;
                SystemCoreClockUpdate();
                return;
            }
        }

        detail::disable_pll();
        RCC->CR &= ~RCC_CR_HSEON;
        RCC->CR |= RCC_CR_HSION;
        detail::wait_mask(RCC->CR, RCC_CR_HSIRDY, detail::kOscSpins);

        RCC->CFGR = (RCC->CFGR & ~(RCC_CFGR_PLLSRC | RCC_CFGR_PLLMUL)) |
                    RCC_CFGR_PLLSRC_HSI_DIV2 | RCC_CFGR_PLLMUL12;
        if (detail::switch_to_pll()) {
            g_sysclk = Sysclk::HsiPll48;
        }

        SystemCoreClockUpdate();
    }

    /**
     * @brief Заморозить IWDG и таймеры 1-Wire/дисплея/тика на halt отладчика.
     * @note Без этого TIM1 продолжает слоты 1-Wire, пока ядро стоит в GDB.
     */
    inline void FreezeDebugPeripherals() {
        RCC->APB2ENR |= RCC_APB2ENR_DBGMCUEN;
        DBGMCU->CR |= DBGMCU_CR_DBG_STOP | DBGMCU_CR_DBG_STANDBY;
        DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_IWDG_STOP | DBGMCU_APB1_FZ_DBG_WWDG_STOP |
                          DBGMCU_APB1_FZ_DBG_TIM3_STOP | DBGMCU_APB1_FZ_DBG_TIM14_STOP;
        DBGMCU->APB2FZ |= DBGMCU_APB2_FZ_DBG_TIM1_STOP | DBGMCU_APB2_FZ_DBG_TIM16_STOP |
                          DBGMCU_APB2_FZ_DBG_TIM17_STOP;
    }

    /** PA8 занят 1-Wire — MCO сюда выводить нельзя. */
    inline void InitMCO() {}

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