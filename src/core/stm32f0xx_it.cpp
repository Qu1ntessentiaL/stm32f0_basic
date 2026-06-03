#include "AppContext.hpp"

extern App app;

volatile uint32_t RccDriver::g_msTicks;

extern "C" {

// void NMI_Handler(void) {}

void HardFault_Handler(void) { while (1) {}}

void SysTick_Handler(void) {
    ++RccDriver::g_msTicks;
    TwiDriver::tick_1ms();
}

void EXTI0_1_IRQHandler(void) {}

void EXTI2_3_IRQHandler(void) {}

void EXTI4_15_IRQHandler(void) {}

void USART1_IRQHandler(void) {
    if (app.uart) {
        app.uart->handleIRQ();
    }
}

void TIM17_IRQHandler(void) {
    if (app.tim17) {
        app.tim17->handleIRQ();
    }
}

} // extern "C"