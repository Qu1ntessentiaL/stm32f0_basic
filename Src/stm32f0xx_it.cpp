#include "TimDriver.hpp"

extern TimDriver *tim17_ptr;

extern "C" {

// void NMI_Handler(void) {}

void HardFault_Handler(void) { while (1) {}}

void SysTick_Handler(void) {}

void EXTI0_1_IRQHandler(void) {}

void EXTI2_3_IRQHandler(void) {}

void EXTI4_15_IRQHandler(void) {}

void USART1_IRQHandler(void) {}

void TIM17_IRQHandler(void) {
    tim17_ptr->handleIRQ();
}

}