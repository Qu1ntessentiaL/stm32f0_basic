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

    uint32_t GetMsTicks();

    inline void InitMax48MHz();

    void InitMCO();

    void IWDG_Init();

    void IWDG_Reload();
}