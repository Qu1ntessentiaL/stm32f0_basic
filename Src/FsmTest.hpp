#pragma once

#include "stm32f0xx.h"
#include "GpioDriver.hpp"
#include "TimDriver.hpp"

class FsmTest {
    GpioDriver *m_gpio;
    TimDriver *m_tim;
public:
    FsmTest(GpioDriver *gpio, TimDriver *tim) : m_gpio(gpio), m_tim(tim) {}
};