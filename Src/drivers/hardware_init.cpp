#include "hardware_init.hpp"
#include "AppContext.hpp"

using namespace RccDriver;

void hardware_init(App& app) {
    InitMax48MHz();
    IWDG_Init();
    SysTick_Config(SystemCoreClock / 1000);

    // UART1
    static UsartDriver<> uart1;
    uart1.Init(SystemCoreClock);
    app.uart = &uart1;

    // I2C1
    static TwiDriver i2c1;
    i2c1.init(SystemCoreClock, 100'000);
    app.twi = &i2c1;

    // TIM17
    static TimDriver tim17(TIM17);
    tim17.Init(47, 999);
    tim17.Start();
    app.tim17 = &tim17;

    // GPIO
    static GpioDriver red_led(GPIOA, 5);
    static GpioDriver green_led(GPIOA, 6);
    static GpioDriver blue_led(GPIOA, 11);
    static GpioDriver light(GPIOB, 0);
    static GpioDriver charger(GPIOA, 15);
    static GpioDriver buzzer(GPIOB, 1);

    red_led.Init(GpioDriver::Mode::Output,
                      GpioDriver::OutType::PushPull,
                      GpioDriver::Pull::None,
                      GpioDriver::Speed::Medium);
    red_led.Reset();

    green_led.Init(GpioDriver::Mode::Alternate,
                        GpioDriver::OutType::PushPull,
                        GpioDriver::Pull::None,
                        GpioDriver::Speed::High);
    green_led.SetAlternateFunction(1); // TIM3_CH1

    blue_led.Init(GpioDriver::Mode::Output,
                  GpioDriver::OutType::PushPull,
                  GpioDriver::Pull::None,
                  GpioDriver::Speed::Medium);
    blue_led.Reset();

    light.Init(GpioDriver::Mode::Output,
               GpioDriver::OutType::PushPull,
               GpioDriver::Pull::None,
               GpioDriver::Speed::Medium);
    light.Reset();

    charger.Init(GpioDriver::Mode::Input);

    buzzer.Init(GpioDriver::Mode::Alternate,
                GpioDriver::OutType::PushPull,
                GpioDriver::Pull::None,
                GpioDriver::Speed::High);
    buzzer.SetAlternateFunction(0); // TIM14_CH1

    app.red_led = &red_led;
    app.green_led = &green_led;
    app.blue_led = &blue_led;
    app.light = &light;
    app.charger = &charger;

    // LCD
    static HT1621B display;
    display.Init();
    app.display = &display;

    // DS18B20 (use PA8, TIM1, DMA1)
    static DS18B20 sensor;
    sensor.init();
    app.sensor = &sensor;

    // PWM-driver for heater
    static PwmDriver heater(TIM3, 1);
    heater.Init(47, 999);
    app.heater = &heater;

    // PWM-driver for buzzer
    static PwmDriver piezo(TIM14, 1);
    piezo.Init(47, 499);
    app.piezo = &piezo;

    // Buttons
    static ButtonsManager btns(GPIOA, 1,
                               GPIOA, 2,
                               GPIOA, 3,
                               GPIOA, 4);
    app.buttons = &btns;
}
