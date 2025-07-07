#include "GpioDriver.hpp"
#include "RccDriver.hpp"
#include "Display.hpp"

int main() {
    RccDriver::InitMax48MHz();
    RccDriver::InitMCO();

    GpioDriver cs(GPIOB, 5),
            wr(GPIOB, 4),
            data(GPIOB, 3);

    while (true) {}
}