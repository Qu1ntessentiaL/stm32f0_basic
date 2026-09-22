#pragma once

struct App;

/**
 *   Инициализация ВСЕГО железа:
 *   - RCC / SysTick
 *   - GPIO (LEDs, charger, light, CS/WCLK/CLK LCD и т.д.)
 *   - UART
 *   - PWM
 *   - таймеры
 *   - дисплей HT1621B
 *   - DS18x20 (DS18B20 / DS18S20)
 *   - кнопки
 *
 *   Watchdog намеренно не запускается здесь: IWDG нельзя остановить,
 *   а hardware_init / services_init могут длиться дольше таймаута.
 */
void hardware_init(App &app);

/** Запуск IWDG. Вызывать после всей инициализации, перед app_loop. */
void watchdog_start();