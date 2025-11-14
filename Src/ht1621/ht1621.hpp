#pragma once

#include "stm32f0xx.h"
#include "GpioDriver.hpp"

class HT1621B {
    struct Segment {
        uint8_t addr;
        uint8_t val;
    };

    enum class WriteMode : uint8_t {
        Replace,
        SetBit,
        ClearBit
    };

    static constexpr Segment m_digits[10][4] = {
            /* 0 */ {{1, 3}, {2, 2}, {3, 1}, {4, 3}},
            /* 1 */ {{1, 0}, {2, 0}, {3, 1}, {4, 2}},
            /* 2 */ {{1, 2}, {2, 3}, {3, 0}, {4, 3}},
            /* 3 */ {{1, 0}, {2, 3}, {3, 1}, {4, 3}},
            /* 4 */ {{1, 1}, {2, 1}, {3, 1}, {4, 2}},
            /* 5 */ {{1, 1}, {2, 3}, {3, 1}, {4, 1}},
            /* 6 */ {{1, 3}, {2, 3}, {3, 1}, {4, 1}},
            /* 7 */ {{1, 0}, {2, 0}, {3, 1}, {4, 3}},
            /* 8 */ {{1, 3}, {2, 3}, {3, 1}, {4, 3}},
            /* 9 */ {{1, 1}, {2, 3}, {3, 1}, {4, 3}}
    };

    // Латинские буквы для 7-сегментного индикатора
    static constexpr Segment m_letters[][4] = {
            /* A */ {{1,3}, {2,1}, {3,1}, {4,3}},
            /* b */ {{1,3}, {2,3}, {3,1}, {4,0}},
            /* C */ {{1,3}, {2,2}, {3,0}, {4,1}},
            /* d */ {{1,2}, {2,3}, {3,1}, {4,2}},
            /* E */ {{1,3}, {2,3}, {3,0}, {4,1}},
            /* F */ {{1,3}, {2,1}, {3,0}, {4,1}},
            /* G */ {{1,3}, {2,2}, {3,1}, {4,1}},
            /* h */ {{1,3}, {2,1}, {3,1}, {4,0}},
            /* I */ {{1,3}, {2,0}, {3,0}, {4,0}},
            /* J */ {{1,0}, {2,2}, {3,1}, {4,2}},
            /* L */ {{1,3}, {2,2}, {3,0}, {4,0}},
            /* n */ {{1,2}, {2,1}, {3,1}, {4,0}},
            /* o */ {{1,2}, {2,3}, {3,1}, {4,0}},
            /* P */ {{1,3}, {2,1}, {3,0}, {4,3}},
            /* r */ {{1,2}, {2,1}, {3,0}, {4,0}},
            /* t */ {{1,3}, {2,3}, {3,0}, {4,0}},
            /* U */ {{1,3}, {2,2}, {3,1}, {4,2}},
            /* X */ {{1,3}, {2,1}, {3,1}, {4,2}},
            /* - */ {{1,0}, {2,1}, {3,0}, {4,0}},
            /* _ */ {{1,0}, {2,2}, {3,0}, {4,0}},
            /* Space {{1,0}, {2,0}, {3,0}, {4,0}}, */
    };

    static constexpr Segment m_dots[5] = {
            {0x03, 0x02},
            {0x07, 0x02},
            {0x0B, 0x02},
            {0x0F, 0x02},
            {0x13, 0x02},
    };

    static constexpr Segment m_chargeLevels[4] = {
            {0x1A, 1},
            {0x1B, 2},
            {0x1B, 1},
            {0x1A, 2},
    };

    static constexpr Segment m_specials[4] = {
            /* ->0<- */ {0x00, 0x01},
            /*  NET  */ {0x00, 0x02},
            /*   k   */ {0x17, 0x02},
            /*   g   */ {0x19, 0x02},
    };

    enum Commands : uint8_t {
        SysDis = 0,
        SysEn,
        LcdOff,
        LcdOn,
        RC256K = 0x18,
        Bias12 = 0x28, // 4 commons option
        Bias13 = 0x29, // 4 commons option
    };

    uint8_t m_vram[32] = {0};

    GpioDriver m_cs_pin, m_write_pin, m_data_pin;

    void WriteBit(uint8_t bit);

    void WriteCommand(Commands cmd);

    void WriteData(uint8_t address, uint8_t data);

    void SetData(uint8_t address, uint8_t data, WriteMode mode = WriteMode::Replace);

public:
    HT1621B();

    void Flush();

    void FullClear(bool flushNow = false);

    void ClearSegArea(bool flushNow = false);

    void Init();

    void ShowDot(uint8_t position, bool enable, bool flushNow = false);

    void ShowSpecial(uint8_t type, bool enable, bool flushNow = false);

    void ShowFull(bool flushNow = false);

    void ShowLetter(uint8_t position, char c, bool flushNow = false);

    void ShowString(const char *str, bool flushNow = false);

    void ShowInt(int value, bool flushNow = false);

    void ShowDigit(uint8_t position, uint8_t digit, bool withDot, bool flushNow = false);

    void ShowChargeLevel(uint8_t level, bool flushNow = false);

    void ShowDate(uint8_t day, uint8_t month, uint8_t year, bool flushNow = false);
};