#include "GpioDriver.hpp"
#include "RccDriver.hpp"
#include "Display.hpp"
#include "UartDriver.hpp"

int main() {
    RccDriver::InitMax48MHz();
    RccDriver::InitMCO(); // MCO connected on R9
    RccDriver::InitSysTickUs(1000, SystemCoreClock);

    GpioDriver s1(GPIOA, 1),
            s2(GPIOA, 2),
            s3(GPIOA, 3),
            s4(GPIOA, 4);

    // I2C1_SDA [PB7] - R17
    // I2C1_SCL [PB6] - R12
    // UART1_RX [PA10] - X2.1
    // UART1_TX [PA9] - X2.2

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

    s1.Init(GpioDriver::Mode::Input);
    s2.Init(GpioDriver::Mode::Input);
    s3.Init(GpioDriver::Mode::Input);
    s4.Init(GpioDriver::Mode::Input);

    GpioDriver cs(GPIOB, 5),
            wr(GPIOB, 4),
            data(GPIOB, 3);

    Display lcd(&cs, &wr, &data);
    lcd.Init();

    lcd.TestFill();
    while (true) {
        __NOP();
    }
}

extern "C" void SysTick_Handler(void) {

}