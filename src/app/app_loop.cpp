#include "app_loop.hpp"
#include "event_dispatcher.hpp"
#include "RccDriver.hpp"
#include "config.h"

#include "AppContext.hpp"

void app_loop(App &app) {
    if (!app.sensor || !app.buttons || !app.queue || !app.ctrl ||
        !app.beep || !app.melody || !app.tim17) {
        RccDriver::IWDG_Reload();
        return;
    }

    // Low-level polling
    app.sensor->poll();
    app.buttons->poll(*app.queue);
    app.ctrl->poll();
    app.beep->poll();
    app.melody->poll();

    // Дублирование действий кнопок через UART (клавиши '1'..'4').
    // Press уходит сразу, release — через UART_BUTTON_PRESS_DURATION_MS.
    // Несколько клавиш подряд держат свои release: одна ячейка затирала предыдущую.
    {
        struct PendingRelease {
            Event event;
            uint32_t deadline;
        };
        constexpr uint8_t MaxPendingReleases = 8;
        static PendingRelease pending[MaxPendingReleases];
        static uint8_t pendingCount = 0;

        const uint32_t now = RccDriver::GetMsTicks();
        uint8_t i = 0;
        while (i < pendingCount) {
            // Вычитание в int32 переживает переполнение миллисекундного счётчика.
            if (static_cast<int32_t>(now - pending[i].deadline) >= 0) {
                app.queue->push(pending[i].event);
                pending[i] = pending[pendingCount - 1];
                --pendingCount;
            } else {
                ++i;
            }
        }

        if (app.uart && app.uart->has_data()) {
            constexpr uint8_t MaxUartEventsPerLoop = 8;
            uint8_t processed = 0;
            while (app.uart->has_data() && processed++ < MaxUartEventsPerLoop) {
                int byte = app.uart->read_byte();
                if (byte < 0) break;

                Event evt_press{EventType::None, 0};
                Event evt_release{EventType::None, 2};

                switch (byte) {
                    case '1':
                        evt_press.type = EventType::ButtonS1;
                        evt_release.type = EventType::ButtonS1;
                        break;
                    case '2':
                        evt_press.type = EventType::ButtonS2;
                        evt_release.type = EventType::ButtonS2;
                        break;
                    case '3':
                        evt_press.type = EventType::ButtonS3;
                        evt_release.type = EventType::ButtonS3;
                        break;
                    case '4':
                        evt_press.type = EventType::ButtonS4;
                        evt_release.type = EventType::ButtonS4;
                        break;
                    default:
                        continue;
                }

                app.queue->push(evt_press);

                if (pendingCount < MaxPendingReleases) {
                    pending[pendingCount++] = {evt_release, now + UART_BUTTON_PRESS_DURATION_MS};
                } else {
                    // Буфер полон — release сразу, чтобы клик не потерялся.
                    app.queue->push(evt_release);
                }
            }
        }
    }

    // TIM17 ≈ 1 кГц: забираем всю пачку, иначе Tick100ms плывёт при долгом цикле
    {
        static uint16_t tick100 = 0;
        tick100 = static_cast<uint16_t>(tick100 + app.tim17->takeIrqCount());
        static uint8_t heartbeat = 0;
        while (tick100 >= APP_LOOP_TICKS_PER_100MS) {
            tick100 = static_cast<uint16_t>(tick100 - APP_LOOP_TICKS_PER_100MS);
            app.queue->push({EventType::Tick100ms, 0});
            // Синий PA11: суперлуп жив (не FSM термостата). 1 Гц.
            if (app.blue_led && ++heartbeat >= 5) {
                heartbeat = 0;
                app.blue_led->Toggle();
            }
        }
    }

    for (uint8_t i = 0; i < APP_LOOP_MAX_EVENTS_PER_ITER; ++i) {
        auto e = app.queue->pop();
        if (!e) break;
        dispatch_event(app, *e);
    }

    // Watchdog
    RccDriver::IWDG_Reload();
}
