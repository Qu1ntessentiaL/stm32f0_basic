#include "GpioDriver.hpp"
#include "GpioDriverCT.hpp"
#include "Button.hpp"
#include "RccDriver.hpp"
#include "TimDriver.hpp"
#include "UsartDriver.hpp"

#include "ds18b20.hpp"
#include "Controller.hpp"

using namespace RccDriver;

TimDriver tim17(TIM17);

Button<> *S1_Ptr = nullptr,
        *S2_Ptr = nullptr,
        *S3_Ptr = nullptr,
        *S4_Ptr = nullptr;

HT1621B *disp_ptr = nullptr;

void Tim17Callback() {
    auto e1 = S1_Ptr->tick();
    auto e2 = S2_Ptr->tick();
    auto e3 = S3_Ptr->tick();
    auto e4 = S4_Ptr->tick();

    switch (e1) {
        case Button<>::Event::Pressed:  // Действие при нажатии
            disp_ptr->Clear();
            disp_ptr->ShowLetter(5, 'h');
            disp_ptr->ShowDigit(4, 1, false);
            disp_ptr->ShowInt(30, true);
            break;
        case Button<>::Event::Held:     // Действие при удержании
            break;
        case Button<>::Event::Released: // Действие при отпускании
            GPIOB->BSRR |= GPIO_BSRR_BR_0;
            break;
        default:
            break;
    }
    /*
    switch (e2) {
        case Button<>::Event::Pressed:  // Действие при нажатии
            disp_ptr->ShowLetter(5, 't');
            disp_ptr->ShowDigit(4, 1, false);
            disp_ptr->ShowInt(30, true);
            break;
        case Button<>::Event::Held:     // Действие при удержании
            break;
        case Button<>::Event::Released: // Действие при отпускании
            GPIOB->BSRR |= GPIO_BSRR_BR_0;
            break;
        default:
            break;
    }

    switch (e3) {
        case Button<>::Event::Pressed:  // Действие при нажатии
            disp_ptr->Clear();
            disp_ptr->ShowLetter(5, 'Y', true);
            break;
        case Button<>::Event::Held:     // Действие при удержании
            break;
        case Button<>::Event::Released: // Действие при отпускании
            GPIOB->BSRR |= GPIO_BSRR_BR_0;
            break;
        default:
            break;
    }

    switch (e4) {
        case Button<>::Event::Pressed:  // Действие при нажатии
            disp_ptr->Clear();
            disp_ptr->ShowLetter(5, 'Y', true);
            break;
        case Button<>::Event::Held:     // Действие при удержании
            break;
        case Button<>::Event::Released: // Действие при отпускании
            GPIOB->BSRR |= GPIO_BSRR_BR_0;
            break;
        default:
            break;
    }
     */
}

int main() {
    InitMax48MHz();
    InitMCO(); // MCO connected to R9 resistor (PA8-pin)
    InitSysTickUs(1000, SystemCoreClock);

    static Button<> BtnS1(GPIOA, 1);
    S1_Ptr = &BtnS1;
    static Button<> BtnS2(GPIOA, 2);
    S2_Ptr = &BtnS2;
    static Button<> BtnS3(GPIOA, 3);
    S3_Ptr = &BtnS3;
    static Button<> BtnS4(GPIOA, 4);
    S4_Ptr = &BtnS4;

    //UsartDriver Usart1;
    //UsartDriver<>::Init(SystemCoreClock);
    /**
     * I2C1_SDA [PB7] - R17
     * I2C1_SCL [PB6] - R12
     * UART1_RX [PA10] - X2.1
     * UART1_TX [PA9] - X2.2
     */

    GpioDriver sda(GPIOB, 7),
            scl(GPIOB, 6);

    GpioDriver r8(GPIOA, 7),
            r10(GPIOA, 11),
            r13(GPIOA, 5),
            r14(GPIOA, 6),
            r18(GPIOA, 0),
            r20(GPIOA, 12);

    GpioDriver light(GPIOB, 0),
            buzzer(GPIOB, 1);

    light.Init(GpioDriver::Mode::Output,
               GpioDriver::OutType::PushPull,
               GpioDriver::Pull::None,
               GpioDriver::Speed::Medium);

    tim17.Init(4799, 10);
    tim17.setCallback(Tim17Callback);
    tim17.Start();

    //ds18b20_init();

    static HT1621B disp;
    disp_ptr = &disp;
    disp_ptr->ShowLetter(5, 't');
    disp_ptr->ShowDigit(4, 0, false);
    disp_ptr->ShowInt(25, true);

    /*
    for (;;) {            // Main event loop (non-blocking, cooperative multitasking)
        ds18b20_poll();   // Poll DS18B20 state machine - advances 1-Wire communication state
        Usart1.poll_tx(); // Poll UART transmission - feeds hardware from buffer
        // Other non-blocking tasks can be added here
    }
     */
    while (true) {}
}