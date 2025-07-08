#include "GpioDriver.hpp"
#include "RccDriver.hpp"
#include "Display.hpp"

int main() {
    RccDriver::InitMax48MHz();
    RccDriver::InitMCO();
    RccDriver::InitSysTickUs(1000, SystemCoreClock);

    GpioDriver cs(GPIOB, 5),
            wr(GPIOB, 4),
            data(GPIOB, 3);

    Display lcd(&cs, &wr, &data);
    lcd.Init();

    lcd.TestFill();
    while (true) {
        __asm volatile ("nop");
    }
}

extern "C" void SysTick_Handler(void) {

}