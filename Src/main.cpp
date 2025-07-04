#include "GpioDriver.hpp"
#include "RccDriver.hpp"

int main() {
    RccDriver::InitMax48MHz();
    RccDriver::InitMCO();

    GpioDriver led0(GPIOB, 1);
    GpioDriver led1(GPIOA, 2);
    GpioDriver led2(GPIOA, 3);

    led0.Init(GpioDriver::Mode::Output,
              GpioDriver::OutType::PushPull,
              GpioDriver::Pull::None,
              GpioDriver::Speed::Low);
    led1.Init(GpioDriver::Mode::Output,
              GpioDriver::OutType::PushPull,
              GpioDriver::Pull::None,
              GpioDriver::Speed::Medium);
    led2.Init(GpioDriver::Mode::Output,
              GpioDriver::OutType::PushPull,
              GpioDriver::Pull::None,
              GpioDriver::Speed::High);

    while (true) {
        led0.Toggle();
        led1.Toggle();
        led2.Toggle();
    }
}