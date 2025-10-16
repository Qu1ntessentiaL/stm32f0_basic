/**
 * I2C1_SDA [PB7] - R17
 * I2C1_SCL [PB6] - R12
 * UART1_RX [PA10] - X2.1
 * UART1_TX [PA9] - X2.2
 * Resistor R8  -> PA7
 * Resistor R10 -> PA11
 * Resistor R13 -> PA5
 * Resistor R14 -> PA6
 * Resistor R18 -> PA0
 * Resistor R20 -> PA12
 */

#include "GpioDriver.hpp"
#include "Button.hpp"
#include "RccDriver.hpp"
#include "TimDriver.hpp"

#include "ht1621.hpp"
#include "ds18b20.hpp"
#include "uart.hpp"

/*
__attribute__((section(".bss")))
stkalign_t main_thread_stack[128 / sizeof(stkalign_t)];

stkalign_t __main_thread_stack_base__ = (stkalign_t) &main_thread_stack[0];
stkalign_t __main_thread_stack_end__ = (stkalign_t) &main_thread_stack[sizeof(main_thread_stack) /
                                                                       sizeof(main_thread_stack[0])];
*/

DS18B20 *sens_ptr = nullptr;

HT1621B *disp_ptr = nullptr;

int main() {
    RccDriver::InitMax48MHz();
    SysTick_Config(SystemCoreClock / 1000);

    static GpioDriver
            light(GPIOB, 0),
            blue_led(GPIOA, 11),
            buzzer(GPIOB, 1);

    light.Init(GpioDriver::Mode::Output,
               GpioDriver::OutType::PushPull,
               GpioDriver::Pull::None,
               GpioDriver::Speed::Medium);
    light.Reset();

    blue_led.Init(GpioDriver::Mode::Output,
               GpioDriver::OutType::PushPull,
               GpioDriver::Pull::None,
               GpioDriver::Speed::Medium);

    static HT1621B disp;
    disp_ptr = &disp;

    static DS18B20 sens{};
    sens_ptr = &sens;
    sens.init();   // Initialize DS18B20 driver (non-blocking)

    hardware_init(); // Initialize hardware peripherals (non-blocking)
    uart_write_str("DS18B20 demo starting...\r\n"); // Enqueue startup message to UART buffer

    Buttons buttons(GPIOA, 1,
                    GPIOA, 2,
                    GPIOA, 3,
                    GPIOA, 4);

    while (true) {
        sens_ptr->poll();
        uart_poll_tx();
    }
}