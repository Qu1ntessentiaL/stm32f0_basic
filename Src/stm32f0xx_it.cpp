#include "stm32f0xx_it.hpp"
#include "AppContext.hpp"

using namespace RccDriver;

namespace RccDriver {
    volatile uint32_t g_msTicks = 0;
}

static TimDriver *tim17_cb = nullptr;

namespace IRQ {
    void registerTim17(TimDriver *drv) {
        tim17_cb = drv;
    }

    TimDriver *getTim17() {
        return tim17_cb;
    }
} // namespace IRQ

extern "C" {

// void NMI_Handler(void) {}

void HardFault_Handler(void) { while (1) {}}

void SysTick_Handler(void) {
    ++g_msTicks;
}

void EXTI0_1_IRQHandler(void) {}

void EXTI2_3_IRQHandler(void) {}

void EXTI4_15_IRQHandler(void) {}

void USART1_IRQHandler(void) {}

void TIM17_IRQHandler(void) {
    auto drv = IRQ::getTim17();
    if (drv) {
        drv->handleIRQ();
    }
}

}