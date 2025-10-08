#pragma once

#include "stm32f0xx.h"
#include "ds18b20.h"

int uart_write_str(const char *s);

void uart_poll_tx(void);

void hardware_init(void);

void ds18b20_led_control(unsigned action);

void uart_write_int(int value);

void ds18b20_temp_ready(int16_t temp);
