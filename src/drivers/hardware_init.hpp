#pragma once

struct App;

/**
 *   Инициализация ВСЕГО железа:
 *   - RCC / SysTick / Watchdog
 *   - GPIO (LEDs, charger, light, CS/WCLK/CLK LCD и т.д.)
 *   - UART
 *   - PWM
 *   - таймеры
 *   - дисплей HT1621B
 *   - DS18B20
 *   - кнопки
 */
void hardware_init(App &app);