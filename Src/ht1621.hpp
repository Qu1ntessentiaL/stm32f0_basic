#include "cstring"
#include "cmath"
#include "stm32f0xx.h"
#include "GpioDriver.hpp"

class HT1621B {
    static constexpr uint8_t Numbers[10] = {0xEB, 0x60, 0xC7, 0xE5,
                                            0x6C, 0xAD, 0xAF, 0xE0,
                                            0xEF, 0xED};

    struct Segment {
        uint8_t addr;
        uint8_t val;
    };

    static constexpr Segment digits[10][4] = {
            {{3, 1}, {2, 2}, {1, 3}, {4, 3}}, // 0
            {{3, 1}, {4, 2}},                                          // 1
            {{1, 2}, {2, 3}, {4, 3}},                      // 2
            {{3, 1}, {2, 3}, {4, 3}},                      // 3
            {{1, 1}, {2, 1}, {3, 1}, {4, 2}}, // 4
            {{1, 1}, {3, 1}, {4, 1}, {2, 3}}, // 5
            {{3, 1}, {4, 1}, {1, 3}, {2, 3}}, // 6
            {{3, 1}, {4, 3}},                                          // 7
            {{3, 1}, {1, 3}, {2, 3}, {4, 3}}, // 8
            {{1, 1}, {3, 1}, {2, 3}, {4, 3}}  // 9
    };

    static constexpr Segment dots[6] = {
            {0x03, 0x03},
            {0x07, 0x03},
            {0x0B, 0x03},
            {0x0F, 0x03},
            {0x13, 0x03},
            {0x17, 0x03},
    };

    static constexpr uint8_t chargeLevels[4][2] = {
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

    void ShowInt(int value, bool flushNow = false);

    // void ShowDouble(double value, uint8_t decimals, bool flushNow = false);

    void ShowFloat(float value, uint8_t decimals, bool flushNow = false);

    void ShowDigit(uint8_t position, uint8_t digit, bool withDot = false);

    void ShowChargeLevel(uint8_t, bool flushNow = false);
};