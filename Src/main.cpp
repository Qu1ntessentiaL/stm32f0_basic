/**
 * I2C1_SDA [PB7] - R17
 * I2C1_SCL [PB6] - R12
 * UART1_RX [PA10] - X2.1
 * UART1_TX [PA9] - X2.2
 */

#include "Scheduler.hpp"

#include "GpioDriver.hpp"
#include "GpioDriverCT.hpp"
#include "Button.hpp"
#include "RccDriver.hpp"
#include "TimDriver.hpp"
#include "UsartDriver.hpp"

#include "ht1621.hpp"
#include "ds18b20.h"
#include "uart.h"

TimDriver tim17(TIM17);

Button<> *S1_Ptr = nullptr;

HT1621B *disp_ptr = nullptr;

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

void task_sensor() { ds18b20_poll(); }

void task_uart() { uart_poll_tx(); }

int main() {
    RccDriver::InitMax48MHz();
    //InitMCO(); // MCO connected to R9 resistor (PA8-pin)
    RccDriver::InitSysTickUs(1000, SystemCoreClock);

    static Button<> BtnS1(GPIOA, 1),
            BtnS2(GPIOA, 2),
            BtnS3(GPIOA, 3),
            BtnS4(GPIOA, 4);

    S1_Ptr = &BtnS1;

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

    tim17.Init(4799, 10);
    tim17.setCallback(Tim17Callback);
    tim17.Start();

    //hardware_init(); // Initialize hardware peripherals (non-blocking)
    //uart_write_str("DS18B20 demo starting...\r\n"); // Enqueue startup message to UART buffer
    ds18b20_init();  // Initialize DS18B20 driver (non-blocking)

    UsartDriver Usart1;
    Usart1.Init(SystemCoreClock);
    Usart1.write_str("1234");

    static HT1621B disp;
    disp_ptr = &disp;
    disp_ptr->ShowLetter(5, 't');
    disp_ptr->ShowDigit(4, 0, false);
    disp_ptr->ShowInt(25, true);

    for (;;) {            // Main event loop (non-blocking, cooperative multitasking)
        ds18b20_poll();   // Poll DS18B20 state machine - advances 1-Wire communication state
        //uart_poll_tx(); // Poll UART transmission - feeds hardware from buffer
        // Other non-blocking tasks can be added here
        Usart1.poll_tx();
    }
}
