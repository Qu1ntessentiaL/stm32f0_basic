#pragma once

#include "ds18b20.hpp"

// #define PRINT_TEMP

void hardware_init();

int uart_write_str(const char *s);

void uart_write_int(int value);

void uart_poll_tx();