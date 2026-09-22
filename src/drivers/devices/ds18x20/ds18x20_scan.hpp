/**
 * @file ds18x20_scan.hpp
 * @brief Блокирующий Search ROM для инвентаризации шины DS18x20.
 *
 * Не часть рабочего FSM: другой паттерн доступа (2 чтения + 1 запись на бит),
 * поэтому реализован GPIO bit-bang. Предназначен только для старта прошивки,
 * до запуска IWDG. После возврата PA8/TIM1 принадлежат скану — вызовите
 * @ref DS18X20::rearm().
 *
 * Сборка тела скана отключается флагом @c DS18X20_BUS_SCAN_ENABLED в config.h
 * (`if constexpr` + `--gc-sections`).
 */

#pragma once

#include "UsartDriver.hpp"

/**
 * @brief Обойти шину командой Search ROM (`0xF0`) и напечатать найденные ROM.
 *
 * Формат строки удобно копировать в @c DS18X20_SENSORS:
 * @code
 *   [0] DS18B20  {{0x28, 0xAA, ...}}
 * @endcode
 *
 * @param uart  Порт для отчёта; @c nullptr — выход без скана.
 * @note Блокирует CPU на время обхода (порядка десятков миллисекунд).
 * @note Не более 8 устройств; при большем числе печатается `(scan truncated)`.
 * @warning Не вызывать из @c app_loop() — нет отдачи watchdog и конфликт с FSM.
 */
void ds18x20_scan_bus(UsartDriver<> *uart);
