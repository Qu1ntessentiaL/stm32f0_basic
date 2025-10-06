#include "TimDriver.hpp"

extern TimDriver tim17;

extern "C" void NMI_Handler(void) {

}

extern "C" void HardFault_Handler(void) {
    while (1) {}
}

extern "C" void SysTick_Handler(void) {}

extern "C" void EXTI0_1_IRQHandler(void) {}

extern "C" void EXTI2_3_IRQHandler(void) {}

extern "C" void EXTI4_15_IRQHandler(void) {}

extern "C" void TIM17_IRQHandler(void) {
    tim17.handleIRQ();
}

extern "C" void USART1_IRQHandler(void) {}
