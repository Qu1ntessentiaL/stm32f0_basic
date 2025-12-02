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
#include "fw_info.h"

#include "GpioDriver.hpp"
#include "Button.hpp"
#include "RccDriver.hpp"

#include "ht1621.hpp"
#include "ds18b20.hpp"
#include "uart.hpp"

#include "TimDriver.hpp"

#include "Controller.hpp"

TimDriver *tim17_ptr = nullptr;

DS18B20 *sens_ptr = nullptr;

HT1621B *disp_ptr = nullptr;

EventQueue *queue_ptr = nullptr;

Controller *ctrl_ptr = nullptr;

PwmDriver *heater_ptr = nullptr;

GpioDriver *red_led_ptr = nullptr,
        *green_led_ptr = nullptr,
        *charger_ptr = nullptr;

void doHeavyWork() {
    static uint8_t i = 0;
    disp_ptr->ShowChargeLevel(i++, true);
    if (i > 3) i = 0;
}

void print_fw_info() {
    // Проверим magic
    if (fw_info.magic != 0xDEADBEEF) {
        uart_write_str("FW info not valid\r\n");
        return;
    }

    uart_write_str("=== Firmware Info ===\r\n");

    // Выводим тег без snprintf
    uart_write_str("Tag: ");
    uart_write_str(fw_info.tag);
    uart_write_str("\r\n");

    // Выводим коммит без snprintf
    uart_write_str("Commit: ");
    uart_write_str(fw_info.commit);
    uart_write_str("\r\n");

    uart_write_str("=====================\r\n");
}

int main() {
    __disable_irq();
    RccDriver::InitMax48MHz();
    RccDriver::IWDG_Init();
    SysTick_Config(SystemCoreClock / 1000);

    static EventQueue queue;
    queue_ptr = &queue;

    static Controller ctrl;
    ctrl_ptr = &ctrl;

    static GpioDriver
            light(GPIOB, 0),
            blue_led(GPIOA, 11),
            green_led(GPIOA, 6),
            red_led(GPIOA, 5),
            buzzer(GPIOB, 1),
            charger(GPIOA, 15);

    green_led_ptr = &green_led;
    red_led_ptr = &red_led;
    charger_ptr = &charger;

    light.Init(GpioDriver::Mode::Output,
               GpioDriver::OutType::PushPull,
               GpioDriver::Pull::None,
               GpioDriver::Speed::Medium);
    light.Reset();

    blue_led.Init(GpioDriver::Mode::Output,
                  GpioDriver::OutType::PushPull,
                  GpioDriver::Pull::None,
                  GpioDriver::Speed::Medium);

    green_led_ptr->Init(GpioDriver::Mode::Alternate,
                        GpioDriver::OutType::PushPull,
                        GpioDriver::Pull::None,
                        GpioDriver::Speed::High);
    green_led_ptr->SetAlternateFunction(1); // TIM3_CH1
    static PwmDriver pwm(TIM3, 1);
    heater_ptr = &pwm;
    heater_ptr->Init(47, 999);

    red_led_ptr->Init(GpioDriver::Mode::Output,
                      GpioDriver::OutType::PushPull,
                      GpioDriver::Pull::None,
                      GpioDriver::Speed::Medium);
    red_led_ptr->Reset();

    charger.Init(GpioDriver::Mode::Input);

    static TimDriver tim17(TIM17);
    //tim17.setCallback(tim17_callback);
    tim17.Init(47, 999);
    tim17.Start();
    tim17_ptr = &tim17;

    static HT1621B disp{};
    disp_ptr = &disp;
    ctrl.init();

    static DS18B20 sens{};
    sens_ptr = &sens;
    sens.init();   // Initialize DS18B20 driver (non-blocking)

    hardware_init(); // Initialize hardware peripherals (non-blocking)
    print_fw_info();
    uart_write_str("DS18B20 demo starting...\r\n"); // Enqueue startup message to UART buffer

    ButtonsManager buttons(GPIOA, 1,
                           GPIOA, 2,
                           GPIOA, 3,
                           GPIOA, 4);
    DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_IWDG_STOP;
    DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM3_STOP;
    DBGMCU->APB2FZ |= DBGMCU_APB2_FZ_DBG_TIM17_STOP;
    __enable_irq();

    while (true) {
        sens_ptr->poll();
        uart_poll_tx();
        buttons.poll(queue);

        if (auto e = queue.pop())
            ctrl.processEvent(*e);
        ctrl.poll();

        if (tim17.getIrqCount() > 0) {
            tim17.decIrqCount();

            static uint8_t tickCounter = 0;
            tickCounter++;

            if (tickCounter >= 100) { // раз в 100 мс
                tickCounter = 0;
                queue_ptr->push({EventType::Tick100ms, 0});
            }

            if (charger_ptr->Read()) {
                static uint8_t subcounter = 0;
                subcounter++;

                if (subcounter >= 100) {
                    subcounter = 0;
                    doHeavyWork();
                }
            } else {
                disp_ptr->ShowChargeLevel(0, true);
            }
        }

        RccDriver::IWDG_Reload();
    }
}