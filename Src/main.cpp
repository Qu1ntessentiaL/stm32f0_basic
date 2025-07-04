#include "GpioDriver.hpp"

using led0 = GpioDriver<GpioPort::A, 0>;
using led1 = GpioDriver<GpioPort::B, 1>;
using led2 = GpioDriver<GpioPort::A, 5>;

int main() {
    led0::Init(led0::Mode::Output,
               led0::OutType::PushPull,
               led0::Pull::None,
               led0::Speed::Medium);
    led1::Init(led1::Mode::Output,
               led1::OutType::OpenDrain,
               led1::Pull::Up,
               led1::Speed::Low);
    led2::Init(led2::Mode::Output,
               led2::OutType::PushPull,
               led2::Pull::None,
               led2::Speed::Medium);
    while (true) {
        led0::Toggle();
        led1::Toggle();
        led2::Toggle();
    }
}