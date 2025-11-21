#pragma once

#include "ds18b20.hpp"

// #define PRINT_TEMP

void hardware_init();

void ds18b20_led_control(unsigned action);

void ds18b20_temp_ready(int16_t temp);

int uart_write_str(const char *s);

void uart_write_int(int value);

void uart_poll_tx();