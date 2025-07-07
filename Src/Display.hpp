#pragma once

#include "GpioDriver.hpp"

#define BIAS05 0x21
#define SYSEN  0x01
#define LCDON  0x03

class Display {
    GpioDriver *m_cs, *m_wr, *m_data;

    inline void writeBit(bool bit) {
        bit ? m_data->Set() : m_data->Reset();

        m_wr->Set();
        m_wr->Reset();
        // Delay 1ms
        m_wr->Set();
    }

    inline void writeCommand(uint8_t cmd) {
        m_cs->Set();
        m_cs->Reset();
        writeBit(true);
        writeBit(false);
        writeBit(false);

        for (uint8_t i = 1; i <= 8; i++) {
            (cmd & 0x80) == 0x80 ? writeBit(true) : writeBit(false);
            cmd <<= 1;
        }
        writeBit(false);
        m_data->Set();
        m_cs->Set();
    }

public:
    explicit Display(GpioDriver *cs, GpioDriver *wr, GpioDriver *data)
            : m_cs(cs), m_wr(wr), m_data(data) {}

    inline void Init() {
        m_cs->Init(GpioDriver::Mode::Output, GpioDriver::OutType::PushPull,
                   GpioDriver::Pull::None, GpioDriver::Speed::Medium);
        m_wr->Init(GpioDriver::Mode::Output, GpioDriver::OutType::PushPull,
                   GpioDriver::Pull::None, GpioDriver::Speed::Medium);
        m_data->Init(GpioDriver::Mode::Output, GpioDriver::OutType::PushPull,
                     GpioDriver::Pull::None, GpioDriver::Speed::Medium);

        writeCommand(BIAS05);
        writeCommand(SYSEN);
        writeCommand(LCDON);
        Clean();
    }

    inline void Clean() {

    }
};