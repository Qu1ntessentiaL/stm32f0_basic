#pragma once

#include "stm32f0xx.h"
#include "GpioDriverCT.hpp"

/**
 * @brief
 */
template<GpioPort Port, uint8_t Pin>
class Button {
    using m_gpio = GpioDriverCT<Port, Pin>;
public:
    constexpr explicit Button(GPIO_TypeDef *port, uint8_t pin) {}
};