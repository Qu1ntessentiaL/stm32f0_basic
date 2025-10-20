#pragma once

#include <cstddef>
#include "stm32f0xx.h"
#include "GpioDriver.hpp"

class TwiDriver {
    __STATIC_FORCEINLINE bool start(uint8_t addr, bool read, uint8_t nbytes);

    __STATIC_FORCEINLINE bool stop();

    __STATIC_FORCEINLINE bool waitTX();

    __STATIC_FORCEINLINE bool waitRX();

    __STATIC_FORCEINLINE bool waitTC();

    __STATIC_FORCEINLINE bool waitSTOP();

    __STATIC_FORCEINLINE bool sendByte(uint8_t data);

    __STATIC_FORCEINLINE uint8_t readByte();

    __STATIC_FORCEINLINE bool waitFlag(uint32_t flag, bool set, uint32_t timeout = 10000);

public:
    __STATIC_FORCEINLINE void init(uint32_t speedHz = 100000);

    __STATIC_FORCEINLINE bool write(uint8_t addr, const uint8_t *data, size_t len);

    __STATIC_FORCEINLINE bool read(uint8_t addr, uint8_t *data, size_t len);

    __STATIC_FORCEINLINE bool writeRead(uint8_t addr, const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len);
};