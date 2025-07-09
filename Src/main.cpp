#include "GpioDriver.hpp"
#include "GpioDriverCT.hpp"
#include "Button.hpp"
#include "RccDriver.hpp"
#include "TimDriver.hpp"

TimDriver tim17(TIM17);

void Tim17Callback() {
    GPIOB->ODR ^= GPIO_ODR_0;
}

using s1 = GpioDriverCT<GpioPort::A, 1>;
using s2 = GpioDriverCT<GpioPort::A, 2>;
using s3 = GpioDriverCT<GpioPort::A, 3>;
using s4 = GpioDriverCT<GpioPort::A, 4>;

int main() {
    RccDriver::InitMax48MHz();
    RccDriver::InitMCO(); // MCO connected to R9
    RccDriver::InitSysTickUs(1000, SystemCoreClock);

    s1::Init(s1::Mode::Input);
    s2::Init(s2::Mode::Input);
    s3::Init(s3::Mode::Input);
    s4::Init(s4::Mode::Input);
    /**
     * I2C1_SDA [PB7] - R17
     * I2C1_SCL [PB6] - R12
     * UART1_RX [PA10] - X2.1
     * UART1_TX [PA9] - X2.2
     */

    GpioDriver sda(GPIOB, 7),
            scl(GPIOB, 6);

    GpioDriver rx(GPIOA, 10),
            tx(GPIOA, 9);

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

    tim17.Init(47999, 999);
    tim17.setCallback(Tim17Callback);
    tim17.Start();

    while (true) {
        __NOP();
    }
}