/**
 * @file ds18x20_hw.hpp
 * @brief Общие константы 1-Wire линии (PA8 / TIM1) для FSM и Search ROM.
 */

#pragma once

#include "config.h"
#include "stm32f0xx.h"

namespace ds18x20_hw {

constexpr uint8_t kOwPin = 8;
constexpr uint32_t kOwPinMask = 1u << kOwPin;
constexpr uint16_t kTimPrescaler =
        static_cast<uint16_t>(SYSTEM_CLOCK_HZ / 1'000'000u - 1u);

constexpr uint8_t kCrcPoly = 0x8C;

constexpr uint32_t kResetMinUs = 480;
constexpr uint32_t kResetMaxUs = 540;
constexpr uint32_t kPresenceMinUs = kResetMinUs + 15 + 60;
constexpr uint32_t kPresenceMaxUs = kResetMaxUs + 60 + 240;

/** rc_w0: пишем 0 в UIF, остальные биты SR не трогаем. */
inline void clear_uif() {
    TIM1->SR = ~TIM_SR_UIF;
}

/**
 * @brief Dallas/Maxim CRC8, LSB first, полином 0x8C.
 */
constexpr uint8_t dallas_crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; ++i) {
        uint8_t in = data[i];
        for (uint8_t b = 0; b < 8; ++b) {
            const uint8_t mix = (crc ^ in) & 1u;
            crc >>= 1;
            if (mix) crc ^= kCrcPoly;
            in >>= 1;
        }
    }
    return crc;
}

} // namespace ds18x20_hw
