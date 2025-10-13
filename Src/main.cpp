/**
 * I2C1_SDA [PB7] - R17
 * I2C1_SCL [PB6] - R12
 * UART1_RX [PA10] - X2.1
 * UART1_TX [PA9] - X2.2
 */

#include "Scheduler.hpp"
#include "EventQueue.hpp"
#include "ThermostatController.hpp"

#include "GpioDriver.hpp"
#include "Button.hpp"
#include "RccDriver.hpp"
#include "TimDriver.hpp"
#include "UsartDriver.hpp"

#include "ht1621.hpp"
#include "ds18b20.hpp"
#include "uart.hpp"

TimDriver tim17(TIM17);

DS18B20 *sens_ptr = nullptr;
/*
void Tim17Callback() {
    auto e1 = S1_Ptr->tick();

    switch (e1) {
        case Button<>::Event::Pressed:  // Действие при нажатии
            break;
        case Button<>::Event::Held:     // Действие при удержании
            break;
        case Button<>::Event::Released: // Действие при отпускании
            break;
        default:
            break;
    }
}
*/

void task_sensor() { sens_ptr->poll(); }

void task_uart() { uart_poll_tx(); }

int main() {
    RccDriver::InitMax48MHz();
    //InitMCO(); // MCO connected to R9 resistor (PA8-pin)
    SysTick_Config(SystemCoreClock / 1000);
    /*
    static Button<> BtnS1(GPIOA, 1),
            BtnS2(GPIOA, 2),
            BtnS3(GPIOA, 3),
            BtnS4(GPIOA, 4);

    S1_Ptr = &BtnS1;
    */
    GpioDriver sda(GPIOB, 7),
            scl(GPIOB, 6);

    GpioDriver r8(GPIOA, 7),
            r10(GPIOA, 11),
            r13(GPIOA, 5),
            r14(GPIOA, 6),
            r18(GPIOA, 0),
            r20(GPIOA, 12);

    static GpioDriver light(GPIOB, 0),
            buzzer(GPIOB, 1);

    light.Init(GpioDriver::Mode::Output,
               GpioDriver::OutType::PushPull,
               GpioDriver::Pull::None,
               GpioDriver::Speed::Medium);
    /*
    tim17.Init(4799, 10);
    tim17.setCallback(Tim17Callback);
    tim17.Start();
    */

    static DS18B20 sens{};
    sens_ptr = &sens;
    sens.init();   // Initialize DS18B20 driver (non-blocking)

    hardware_init(); // Initialize hardware peripherals (non-blocking)
    uart_write_str("DS18B20 demo starting...\r\n"); // Enqueue startup message to UART buffer
    /*
    UsartDriver Usart1;
    Usart1.Init(SystemCoreClock);
    Usart1.write_str("1234");
    */
    static HT1621B disp;
    disp.ShowLetter(5, 't');
    disp.ShowDigit(4, 0, false);
    disp.ShowInt(25, true);

    EventQueue eventQueue;
    ThermostatController controller;
    Buttons buttons(GPIOA, 1,
                    GPIOA, 2,
                    GPIOA, 3,
                    GPIOA, 4);

    TaskScheduler scheduler;
    scheduler.init();
    scheduler.addTask(task_uart, 50);
    scheduler.addTask(task_sensor, 5);
    scheduler.run();
}
