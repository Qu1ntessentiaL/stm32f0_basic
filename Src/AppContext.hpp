#pragma once

#include <cstdint>

#include "UsartDriver.hpp"
#include "TimDriver.hpp"
#include "EventQueue.hpp"
#include "ht1621.hpp"
#include "ds18b20.hpp"

struct App {
    DS18B20 *sens;
    HT1621B *disp;
    EventQueue *queue;
    GpioDriver *red_led;
    GpioDriver *green_led;
    GpioDriver *charger;
    UsartDriver<> *uart;
};

extern volatile uint32_t g_msTicks;

inline uint32_t GetMsTicks() {
    return g_msTicks;
}