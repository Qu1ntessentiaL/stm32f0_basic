#include "TimDriver.hpp"
#include "SystemTime.h"

volatile uint32_t g_msTicks = 0;

extern TimDriver *tim17_ptr;

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
    if (tim17_ptr) {
        tim17_ptr->handleIRQ();
    }
}

}