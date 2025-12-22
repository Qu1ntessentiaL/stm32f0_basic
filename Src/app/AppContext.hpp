#pragma once

#include "UsartDriver.hpp"
#include "TwiDriver.hpp"
#include "TimDriver.hpp"
#include "GpioDriver.hpp"
#include "ht1621.hpp"
#include "ds18b20.hpp"
#include "ButtonsManager.hpp"
#include "Controller.hpp"

/**
 *   Контекст приложения (Dependency Injection Container)
 */
struct App {
    // Hardware-level (низкоуровневые драйверы)
    UsartDriver<>   *uart = nullptr;
    TwiDriver       *twi = nullptr;
    TimDriver       *tim17 = nullptr;
    DS18B20         *sensor = nullptr;
    HT1621B         *display = nullptr;
    GpioDriver      *red_led = nullptr;
    GpioDriver      *green_led = nullptr;
    GpioDriver      *blue_led = nullptr;
    GpioDriver      *light = nullptr;
    GpioDriver      *charger = nullptr;
    PwmDriver       *heater = nullptr;
    PwmDriver       *piezo = nullptr;
    ButtonsManager  *buttons = nullptr;

    // Application-level services
    EventQueue      *queue = nullptr;
    BeepManager     *beep = nullptr;
    Controller      *ctrl = nullptr;
};