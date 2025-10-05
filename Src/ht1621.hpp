#include "stm32f0xx.h"
#include "cmath"

namespace LCD {
    void WriteData(uint8_t address, uint8_t data);

    void WriteFull();

    void Clean();

    void Init();

    void ShowInt(int);

    void ShowDouble(double);

    void ShowDigit(uint8_t, uint8_t);

    void ShowDigitDot(uint8_t, uint8_t);

    void ShowChargeLevel(uint8_t);
}