#include "fw_info.h"

#include "stm32f0xx_it.hpp"

#include "AppContext.hpp"
#include "Controller.hpp"
#include "RccDriver.hpp"
#include "Button.hpp"

App app{};

void doHeavyWork() {
    static uint8_t i = 0;
    app.disp->ShowChargeLevel(i++, true);
    if (i > 3) i = 0;
}

int main() {
    __disable_irq();
    RccDriver::InitMax48MHz();
    RccDriver::IWDG_Init();
    SysTick_Config(SystemCoreClock / 1000);

    static GpioDriver
            light(GPIOB, 0),
            blue_led(GPIOA, 11),
            green_led(GPIOA, 6),
            red_led(GPIOA, 5),
            buzzer(GPIOB, 1),
            charger(GPIOA, 15);

    buzzer.Init(GpioDriver::Mode::Alternate,
                GpioDriver::OutType::PushPull,
                GpioDriver::Pull::None,
                GpioDriver::Speed::High);
    buzzer.SetAlternateFunction(0); // TIM14_CH1

    static PwmDriver piezo(TIM14, 1);
    piezo.Init(47, 499);

    static BeepManager beep(piezo);

    static EventQueue queue;
    app.queue = &queue;

    static HT1621B disp{};
    app.disp = &disp;

    static PwmDriver pwm(TIM3, 1);
    pwm.Init(47, 999);

    static Controller ctrl(&disp, &beep, &pwm);

    app.green_led = &green_led;
    app.red_led = &red_led;
    app.charger = &charger;

    light.Init(GpioDriver::Mode::Output,
               GpioDriver::OutType::PushPull,
               GpioDriver::Pull::None,
               GpioDriver::Speed::Medium);
    light.Reset();

    blue_led.Init(GpioDriver::Mode::Output,
                  GpioDriver::OutType::PushPull,
                  GpioDriver::Pull::None,
                  GpioDriver::Speed::Medium);

    app.green_led->Init(GpioDriver::Mode::Alternate,
                        GpioDriver::OutType::PushPull,
                        GpioDriver::Pull::None,
                        GpioDriver::Speed::High);
    app.green_led->SetAlternateFunction(1); // TIM3_CH1


    app.red_led->Init(GpioDriver::Mode::Output,
                      GpioDriver::OutType::PushPull,
                      GpioDriver::Pull::None,
                      GpioDriver::Speed::Medium);
    app.red_led->Reset();

    charger.Init(GpioDriver::Mode::Input);

    static TimDriver tim17(TIM17);
    tim17.Init(47, 999);
    tim17.Start();
    IRQ::registerTim17(&tim17);

    ctrl.init();

    static DS18B20 sens{};
    app.sens = &sens;
    sens.init();   // Initialize DS18B20 driver (non-blocking)

    static UsartDriver<> uart1;
    uart1.Init(SystemCoreClock);
    app.uart = &uart1;
    print_fw_info(&uart1);
    uart1.write_str("DS18B20 demo starting...\r\n"); // Enqueue startup message to UART buffer

    ButtonsManager buttons(GPIOA, 1,
                           GPIOA, 2,
                           GPIOA, 3,
                           GPIOA, 4);

    DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_IWDG_STOP;
    DBGMCU->APB1FZ |= DBGMCU_APB1_FZ_DBG_TIM3_STOP;
    DBGMCU->APB2FZ |= DBGMCU_APB2_FZ_DBG_TIM17_STOP;
    __enable_irq();

    while (true) {
        app.sens->poll();
        uart1.poll_tx();
        buttons.poll(queue);
        ctrl.poll();
        beep.poll();

        if (tim17.getIrqCount() > 0) {
            tim17.decIrqCount();

            static uint8_t tickCounter = 0;
            tickCounter++;

            if (tickCounter >= 100) { // раз в 100 мс
                tickCounter = 0;
                app.queue->push({EventType::Tick100ms, 0});
            }

            if (app.charger->Read()) {
                static uint8_t subcounter = 0;
                subcounter++;

                if (subcounter >= 100) {
                    subcounter = 0;
                    doHeavyWork();
                }
            } else {
                app.disp->ShowChargeLevel(0, true);
            }
        }

        if (auto e = queue.pop()) {
            if (e->type == EventType::ButtonS3 && e->value == 0) {
                uart1.write_str("S1 pressed -> beep\r\n");
                piezo.setPower(500);
            }

            if (e->type == EventType::ButtonS3 && e->value == 2) {
                uart1.write_str("S1 released-> stop beep\r\n");
                piezo.setPower(0);
            }

            ctrl.processEvent(*e);
        }

        RccDriver::IWDG_Reload();
    }
}