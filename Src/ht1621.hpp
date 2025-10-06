#include "cmath"
#include "stm32f0xx.h"
#include "GpioDriver.hpp"

class LCD {
    constexpr static const uint8_t Full[32] = {0xff, 0xff, 0xff, 0xff,
                                               0xff, 0xff, 0xff, 0xff,
                                               0xff, 0xff, 0xff, 0xff,
                                               0xff, 0xff, 0xff, 0xff,
                                               0xff, 0xff, 0xff, 0xff,
                                               0xff, 0xff, 0xff, 0xff,
                                               0xff, 0xff, 0xff, 0xff,
                                               0xff, 0xff, 0xff, 0xff};
    constexpr static const uint8_t Numbers[10] = {0xEB, 0x60, 0xC7, 0xE5,
                                                  0x6C, 0xAD, 0xAF, 0xE0,
                                                  0xEF, 0xED};

    enum Commands : uint8_t {
        SysDis = 0, SysEn, LcdOff, LcdOn, Bias05 = 0x21
    };

    GpioDriver m_cs_pin, m_write_pin, m_data_pin;

    void WriteBit(uint8_t bit);

    void WriteCommand(Commands cmd);

    void WriteData(uint8_t address, uint8_t data);

public:
    LCD();

    void WriteFull();

    void Clean();

    void Init();

    void ShowInt(int);

    void ShowDouble(double);

    void ShowDigit(uint8_t, uint8_t);

    void ShowDigitDot(uint8_t, uint8_t);

    void ShowChargeLevel(uint8_t);
};