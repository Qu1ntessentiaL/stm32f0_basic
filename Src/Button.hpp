#pragma once

#include "stm32f0xx.h"

/**
 * @brief
 */
class Button {
    GpioDriver m_gpio;
public:
    constexpr explicit Button(GpioDriver gpio) : m_gpio(gpio) {

    }
};