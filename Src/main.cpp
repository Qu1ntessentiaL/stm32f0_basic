#include "GpioDriver.hpp"
#include "GpioDriverCT.hpp"
#include "Button.hpp"
#include "RccDriver.hpp"
#include "TimDriver.hpp"
#include "UsartDriver.hpp"

TimDriver tim17(TIM17);

using ButtonS1 = Button<GpioPort::A, 1, 5, 100>;
using ButtonS2 = Button<GpioPort::A, 2>;
using ButtonS3 = Button<GpioPort::A, 3>;
using ButtonS4 = Button<GpioPort::A, 4>;

using Usart1 = UsartDriver<
        UsartInstance::COM1,
        GpioPort::A, 10,
        GpioPort::A, 9,
        115200
>;

void Tim17Callback() {
    auto e = ButtonS1::tick();

    switch (e) {
        case ButtonS1::Event::Pressed: // Действие при нажатии
            GPIOB->BSRR |= GPIO_BSRR_BS_0;
            break;
        case ButtonS1::Event::Held: // Действие при удержании
            break;
        case ButtonS1::Event::Released: // Действие при отпускании
            GPIOB->BSRR |= GPIO_BSRR_BR_0;
            break;
        default:
            break;
    }
}

int main() {
    RccDriver::InitMax48MHz();
    RccDriver::InitMCO(); // MCO connected to R9
    RccDriver::InitSysTickUs(1000, SystemCoreClock);

    Usart1::Init(SystemCoreClock);
    Usart1::SendString("Hello from Usart1!\r\n");
    ButtonS1::Init();
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

    GpioDriver cs(GPIOB, 5),
            wr(GPIOB, 4),
            data(GPIOB, 3);

    //tim17.Init(47999, 1);
    //tim17.setCallback(Tim17Callback);
    //tim17.Start();

    while (true) {
        __NOP();
    }
}