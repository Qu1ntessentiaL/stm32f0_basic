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
#include <cstdio>
#include "fw_info.h"

#include "GpioDriver.hpp"
#include "Button.hpp"
#include "RccDriver.hpp"

#include "ht1621.hpp"
#include "ds18b20.hpp"
#include "uart.hpp"

#include "my_fsm.hpp"
#include "TimDriver.hpp"

TimDriver *tim17_ptr = nullptr;

DS18B20 *sens_ptr = nullptr;

HT1621B *disp_ptr = nullptr;

void tim17_callback() {
    __NOP();
}

void print_fw_info() {
    char buf[128];

    // Проверим magic
    if (fw_info.magic != 0xDEADBEEF) {
        uart_write_str("FW info not valid\r\n");
        return;
    }

    uart_write_str("=== Firmware Info ===\r\n");

    snprintf(buf, sizeof(buf), "Tag: %s\r\n", fw_info.tag);
    uart_write_str(buf);

    snprintf(buf, sizeof(buf), "Commit: %s\r\n", fw_info.commit);
    uart_write_str(buf);

    uart_write_str("=====================\r\n");
}

int main() {
    RccDriver::InitMax48MHz();
    RccDriver::IWDG_Init();
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

    static TimDriver tim17(TIM17);
    tim17.setCallback(tim17_callback);
    tim17.Init(47999, 500);
    tim17.Start();
    tim17_ptr = &tim17;

    static HT1621B disp;
    disp_ptr = &disp;

    static DS18B20 sens{};
    sens_ptr = &sens;
    sens.init();   // Initialize DS18B20 driver (non-blocking)

    hardware_init(); // Initialize hardware peripherals (non-blocking)
    print_fw_info();
    uart_write_str("DS18B20 demo starting...\r\n"); // Enqueue startup message to UART buffer

    __unused Buttons buttons(GPIOA, 1,
                             GPIOA, 2,
                             GPIOA, 3,
                             GPIOA, 4);
    disp_ptr->ShowDate(22, 12, 94);

    while (true) {
        sens_ptr->poll();
        uart_poll_tx();
        RccDriver::IWDG_Reload();
    }
}