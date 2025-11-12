#pragma once

#include <cstdint>

extern volatile uint32_t g_msTicks;

inline uint32_t GetMsTicks() {
    return g_msTicks;
}