#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stm32f0xx.h"
#include "ds18b20.h"

void hardware_init(void);

void ds18b20_led_control(unsigned action);

void ds18b20_temp_ready(int16_t temp);

int uart_write_str(const char *s);

void uart_write_int(int value);

void uart_poll_tx(void);

#ifdef __cplusplus
}
#endif
