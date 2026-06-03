#include "app_loop.hpp"
#include "event_dispatcher.hpp"
#include "RccDriver.hpp"
#include "config.h"

#include "AppContext.hpp"

#include <optional>

void app_loop(App &app) {
    // Low-level polling
    app.sensor->poll();
    app.buttons->poll(*app.queue);
    app.ctrl->poll();
    app.beep->poll();

    // Дублирование действий кнопок через UART (клавиши '1'..'4')
    // Добавляем небольшую «длительность» нажатия, чтобы звук был слышен:
    // отправляем press сразу, release — через ~50 мс.
    {
        static std::optional<Event> pending_uart_release;
        static uint32_t pending_uart_release_deadline = 0;

        // Завершаем отложенный release, если подошло время
        if (pending_uart_release && RccDriver::GetMsTicks() >= pending_uart_release_deadline) {
            app.queue->push(*pending_uart_release);
            pending_uart_release.reset();
        }

        if (app.uart && app.queue && app.uart->has_data()) {
            while (app.uart->has_data()) {
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

                // Отправляем press сразу
                app.queue->push(evt_press);

                // Планируем release (или заменяем предыдущий отложенный)
                pending_uart_release = evt_release;
                pending_uart_release_deadline = RccDriver::GetMsTicks() + UART_BUTTON_PRESS_DURATION_MS;
            }
        }
    }

    // Timer 100 ms
    if (app.tim17->getIrqCount()) {
        app.tim17->decIrqCount();

        static uint8_t tick100 = 0;
        if (++tick100 >= APP_LOOP_TICKS_PER_100MS) {
            tick100 = 0;
            app.queue->push({EventType::Tick100ms, 0});
        }
    }

    // Event processing
    if (auto e = app.queue->pop()) {
        dispatch_event(app, *e);
    }

    // Watchdog
    RccDriver::IWDG_Reload();
}
