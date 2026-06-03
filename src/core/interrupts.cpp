#include "AppContext.hpp"

extern App app;

volatile uint32_t RccDriver::g_msTicks;

extern "C" {

__NO_RETURN
void NMI_Handler(void) {
    while (true) {}
}

__USED __NO_RETURN
void HardFault_C_Handler(const uint32_t *stacked) {
    volatile uint32_t r0 = stacked[0];
    volatile uint32_t r1 = stacked[1];
    volatile uint32_t r2 = stacked[2];
    volatile uint32_t r3 = stacked[3];
    volatile uint32_t r12 = stacked[4];
    volatile uint32_t lr = stacked[5];
    volatile uint32_t pc = stacked[6];
    volatile uint32_t psr = stacked[7];

    (void)r0;
    (void)r1;
    (void)r2;
    (void)r3;
    (void)r12;
    (void)lr;
    (void)pc;
    (void)psr;

    while (true) {}
}

__attribute__((naked))
void HardFault_Handler(void) {
    __asm volatile(
        "mrs r0, msp\n"
        "b HardFault_C_Handler\n"
    );
}

void SVC_Handler(void) {}

void PendSV_Handler(void) {}

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