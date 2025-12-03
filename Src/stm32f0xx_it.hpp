#pragma once

#include <cstdint>

class TimDriver;

namespace IRQ {
    void registerTim17(TimDriver *drv);

    TimDriver *getTim17();
}