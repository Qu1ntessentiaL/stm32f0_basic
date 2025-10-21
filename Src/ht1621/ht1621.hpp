#pragma once

#include "cstring"
#include "cmath"
#include "stm32f0xx.h"
#include "GpioDriver.hpp"

class HT1621B {
    struct Segment {
        uint8_t addr;
        uint8_t val;
    };

    static constexpr Segment m_digits[10][4] = {
            /* 0 */ {{3, 1}, {2, 2}, {1, 3}, {4, 3}},
            /* 1 */ {{3, 1}, {4, 2}},
            /* 2 */ {{1, 2}, {2, 3}, {4, 3}},
            /* 3 */ {{3, 1}, {2, 3}, {4, 3}},
            /* 4 */ {{1, 1}, {2, 1}, {3, 1}, {4, 2}},
            /* 5 */ {{1, 1}, {3, 1}, {4, 1}, {2, 3}},
            /* 6 */ {{3, 1}, {4, 1}, {1, 3}, {2, 3}},
            /* 7 */ {{3, 1}, {4, 3}},
            /* 8 */ {{3, 1}, {1, 3}, {2, 3}, {4, 3}},
            /* 9 */ {{1, 1}, {3, 1}, {2, 3}, {4, 3}}
    };

    // Латинские буквы для 7-сегментного индикатора
    static constexpr Segment m_letters[][5] = {
            /* A */ {{1,3}, {2,1}, {3,1}, {4,3}},
            /* b */ {{1,3}, {2,3}, {3,1}},
            /* C */ {{1,3}, {2,2}, {4,1}},
            /* d */ {{1,2}, {2,3}, {3,1}, {4,2}},
            /* E */ {{1,3}, {2,3}, {4,1}},
            /* F */ {{1,3}, {2,1}, {4,1}},
            /* G */ {{1,3}, {2,2}, {3,1}, {4,1}},
            /* h */ {{1,3}, {2,1}, {3,1}},
            /* I */ {{3,1}, {4,2}},
            /* J */ {{2,2}, {3,1}, {4,1}},
            /* L */ {{1,3}, {2,2}},
            /* n */ {{2,2}, {3,1}, {1,3}},
            /* o */ {{1,1}, {2,3}, {3,1}},
            /* P */ {{1,3}, {2,1}, {4,3}},
            /* r */ {{1,2}, {2,1}},
            /* S */ {{1,1}, {2,3}, {3,1}, {4,1}},
            /* t */ {{1,3}, {2,3}},
            /* U */ {{1,3}, {2,2}, {3,1}, {4,1}},
            /* - */ {{2,1}},
            /* _ */ {{2,2}},
    };

    static constexpr Segment m_dots[6] = {
            {0x03, 0x02},
            {0x07, 0x02},
            {0x0B, 0x02},
            {0x0F, 0x02},
            {0x13, 0x02},
            {0x17, 0x02},
    };

    static constexpr uint8_t m_chargeLevels[4][2] = {
            {1, 0},
            {1, 2},
            {1, 3},
            {3, 3}
    };

    enum Commands : uint8_t {
        SysDis = 0,
        SysEn,
        LcdOff,
        LcdOn,
        RC256K = 0x18,
        Bias05 = 0x28, // 4 commons option
        Bias13 = 0x29, // 4 commons option
    };

    uint8_t m_vram[32] = {0};

    GpioDriver m_cs_pin, m_write_pin, m_data_pin;

    void WriteBit(uint8_t bit);

    void WriteCommand(Commands cmd);

    void WriteData(uint8_t address, uint8_t data);

public:
    HT1621B();

    void SetData(uint8_t address, uint8_t data);

    void Flush();

    void Clear(bool flushNow = false);

    void Init();

    void ShowFull(bool flushNow = false);

    void ShowLetter(uint8_t position, char c, bool flushNow = false);

    void ShowInt(int value, bool flushNow = false);

    // void ShowFloat(float value, uint8_t decimals, bool flushNow = false);

    void ShowDigit(uint8_t position, uint8_t digit, bool withDot, bool flushNow = false);

    void ShowChargeLevel(uint8_t, bool flushNow = false);

    void ShowDate(uint8_t day, uint8_t month, uint8_t year, bool flushNow = false);
};