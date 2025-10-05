#include "ht1621.hpp"

#define SYSDIS    0x00
#define SYSEN     0x01
#define LCDOFF    0x02
#define LCDON     0x03
#define BIAS05    0x21

#define CS_LOW    SET_BIT(GPIOB->BSRR, GPIO_BSRR_BR_5)
#define CS_HIGH   SET_BIT(GPIOB->BSRR, GPIO_BSRR_BS_5)
#define RD_LOW    SET_BIT(GPIOB->BSRR, GPIO_BSRR_BR_6)
#define RD_HIGH   SET_BIT(GPIOB->BSRR, GPIO_BSRR_BS_6)
#define WR_LOW    SET_BIT(GPIOB->BSRR, GPIO_BSRR_BR_4)
#define WR_HIGH   SET_BIT(GPIOB->BSRR, GPIO_BSRR_BS_4)
#define DATA_LOW  SET_BIT(GPIOB->BSRR, GPIO_BSRR_BR_3)
#define DATA_HIGH SET_BIT(GPIOB->BSRR, GPIO_BSRR_BS_3)

static const uint8_t Full[32] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff,
                                 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};
static const uint8_t Number[10] = {0xEB, 0x60, 0xC7, 0xE5,
                                   0x6C, 0xAD, 0xAF, 0xE0, 0xEF,
                                   0xED};

namespace LCD {
    static void WriteBit(uint8_t bit) {
        uint8_t i;
        if (bit)
            DATA_HIGH;
        else
            DATA_LOW;
        WR_HIGH;
        WR_LOW;
        for (i = 0; i <= 10; i++);
        WR_HIGH;
    }

    static void WriteCommand(uint8_t cmd) {
        unsigned char i;
        CS_HIGH;
        CS_LOW;
        WriteBit(1);
        WriteBit(0);
        WriteBit(0);
        for (i = 0; i < 8; i++) {
            if ((cmd & 0x80) == 0x80)
                WriteBit(1);
            else
                WriteBit(0);
            cmd <<= 1;
        }
        WriteBit(0);
        DATA_HIGH;
        CS_HIGH;
    }

    void WriteData(uint8_t address, uint8_t data) {
        unsigned char i;
        CS_HIGH;
        CS_LOW;
        WriteBit(1);
        WriteBit(0);
        WriteBit(1);
        address <<= 2;
        for (i = 0; i < 6; i++) {
            if ((address & 0x80) == 0x80)
                WriteBit(1);
            else
                WriteBit(0);
            address <<= 1;
        }
        for (i = 0; i < 4; i++) {
            if ((data & 0x01) == 0x01) {
                WriteBit(1);
            } else {
                WriteBit(0);
            }
            data >>= 1;
        }
        DATA_HIGH;
        CS_HIGH;
    }

    void WriteFull() {
        uint8_t j;
        for (j = 0; j < 32; j++) {
            WriteData(j, Number[j]);
        }
    }

    void Clean() {
        uint8_t j;
        for (j = 0; j < 28; j++) {
            WriteData(j, 0x00);
        }
    }

    void Init() {
        WriteCommand(BIAS05);
        WriteCommand(SYSEN);
        WriteCommand(LCDON);
        Clean();
    }

/*
0
	lcdwd(3, 1);
	lcdwd(2, 2);
	lcdwd(1, 3);
	lcdwd(4, 3);
1
	lcdwd(3, 1);
	lcdwd(4, 2);
2
	lcdwd(1, 2);
	lcdwd(2, 3);
	lcdwd(4, 3);
3
	lcdwd(3, 1);
	lcdwd(2, 3);
	lcdwd(4, 3);
4
	lcdwd(1, 1);
	lcdwd(2, 1);
	lcdwd(3, 1);
	lcdwd(4, 2);
5
	lcdwd(1, 1);
	lcdwd(3, 1);
	lcdwd(4, 1);
	lcdwd(2, 3);
6
	lcdwd(3, 1);
	lcdwd(4, 1);
	lcdwd(1, 3);
	lcdwd(2, 3);
7
	lcdwd(3, 1);
	lcdwd(4, 3);
8
	lcdwd(3, 1);
	lcdwd(1, 3);
	lcdwd(2, 3);
	lcdwd(4, 3);
9
	lcdwd(1, 1);
	lcdwd(3, 1);
	lcdwd(2, 3);
	lcdwd(4, 3);
*/

    void ShowDigit(uint8_t position, uint8_t digit) {
        switch (digit) {
            case 0 :
                WriteData(3 + (6 - position) * 4, 1);
                WriteData(2 + (6 - position) * 4, 2);
                WriteData(1 + (6 - position) * 4, 3);
                WriteData(4 + (6 - position) * 4, 3);
                break;
            case 1 :
                WriteData(3 + (6 - position) * 4, 1);
                WriteData(4 + (6 - position) * 4, 2);
                break;
            case 2 :
                WriteData(1 + (6 - position) * 4, 2);
                WriteData(2 + (6 - position) * 4, 3);
                WriteData(4 + (6 - position) * 4, 3);
                break;
            case 3 :
                WriteData(3 + (6 - position) * 4, 1);
                WriteData(2 + (6 - position) * 4, 3);
                WriteData(4 + (6 - position) * 4, 3);
                break;
            case 4 :
                WriteData(1 + (6 - position) * 4, 1);
                WriteData(2 + (6 - position) * 4, 1);
                WriteData(3 + (6 - position) * 4, 1);
                WriteData(4 + (6 - position) * 4, 2);
                break;
            case 5 :
                WriteData(1 + (6 - position) * 4, 1);
                WriteData(3 + (6 - position) * 4, 1);
                WriteData(4 + (6 - position) * 4, 1);
                WriteData(2 + (6 - position) * 4, 3);
                break;
            case 6 :
                WriteData(3 + (6 - position) * 4, 1);
                WriteData(4 + (6 - position) * 4, 1);
                WriteData(1 + (6 - position) * 4, 3);
                WriteData(2 + (6 - position) * 4, 3);
                break;
            case 7 :
                WriteData(3 + (6 - position) * 4, 1);
                WriteData(4 + (6 - position) * 4, 3);
                break;
            case 8 :
                WriteData(3 + (6 - position) * 4, 1);
                WriteData(1 + (6 - position) * 4, 3);
                WriteData(2 + (6 - position) * 4, 3);
                WriteData(4 + (6 - position) * 4, 3);
                break;
            case 9 :
                WriteData(1 + (6 - position) * 4, 1);
                WriteData(3 + (6 - position) * 4, 1);
                WriteData(2 + (6 - position) * 4, 3);
                WriteData(4 + (6 - position) * 4, 3);
                break;
        }
    }

    void ShowDigitDot(uint8_t position, uint8_t digit) {
        switch (digit) {
            case 0 :
                WriteData(3 + (6 - position) * 4, 3);
                WriteData(2 + (6 - position) * 4, 2);
                WriteData(1 + (6 - position) * 4, 3);
                WriteData(4 + (6 - position) * 4, 3);
                break;
            case 1 :
                WriteData(3 + (6 - position) * 4, 3);
                WriteData(4 + (6 - position) * 4, 2);
                break;
            case 2 :
                WriteData(1 + (6 - position) * 4, 2);
                WriteData(2 + (6 - position) * 4, 3);
                WriteData(4 + (6 - position) * 4, 3);
                break;
            case 3 :
                WriteData(3 + (6 - position) * 4, 3);
                WriteData(2 + (6 - position) * 4, 3);
                WriteData(4 + (6 - position) * 4, 3);
                break;
            case 4 :
                WriteData(1 + (6 - position) * 4, 1);
                WriteData(2 + (6 - position) * 4, 1);
                WriteData(3 + (6 - position) * 4, 3);
                WriteData(4 + (6 - position) * 4, 2);
                break;
            case 5 :
                WriteData(1 + (6 - position) * 4, 1);
                WriteData(3 + (6 - position) * 4, 3);
                WriteData(4 + (6 - position) * 4, 1);
                WriteData(2 + (6 - position) * 4, 3);
                break;
            case 6 :
                WriteData(3 + (6 - position) * 4, 3);
                WriteData(4 + (6 - position) * 4, 1);
                WriteData(1 + (6 - position) * 4, 3);
                WriteData(2 + (6 - position) * 4, 3);
                break;
            case 7 :
                WriteData(3 + (6 - position) * 4, 3);
                WriteData(4 + (6 - position) * 4, 3);
                break;
            case 8 :
                WriteData(3 + (6 - position) * 4, 3);
                WriteData(1 + (6 - position) * 4, 3);
                WriteData(2 + (6 - position) * 4, 3);
                WriteData(4 + (6 - position) * 4, 3);
                break;
            case 9 :
                WriteData(1 + (6 - position) * 4, 1);
                WriteData(3 + (6 - position) * 4, 3);
                WriteData(2 + (6 - position) * 4, 3);
                WriteData(4 + (6 - position) * 4, 3);
                break;
        }
    }

    void ShowInt(int num) {
        uint8_t j, i = 0;
        for (j = 0; j <= 27; j++) {
            WriteData(j, 0x00);
        }
        if (num < 0) {
            //sign = 1;
            num *= -1;
        }
        do {
            ShowDigit(++i, num % 10);
            num /= 10;
        } while (num);
    }

    void ShowDouble(double m) {
        uint8_t sign = 0, i = 1, j = 1;
        long n;
        if (m < 0) {
            sign = 1;
            m *= -1;
        }
        if (floor(m) != 0) {
            while (m - floor(m) > 0) {
                j++;
                m *= 10;
            }
            n = (long) m;
            do {
                if (i == j) {
                    ShowDigitDot(j, n % 10);
                    n /= 10;
                    i++;
                } else {
                    ShowDigit(i, n % 10);
                    n /= 10;
                    i++;
                }
            } while (n);
            if (sign) {
                WriteData(2 + (6 - i) * 4, 1);
            }
        } else {
            m += 1;
            while (m - floor(m) > 0) {
                j++;
                m *= 10;
            }
            n = (long) m;
            do {
                if (i == j) {
                    ShowDigitDot(j, n % 10 - 1);
                    n /= 10;
                    i++;
                } else {
                    ShowDigit(i, n % 10);
                    n /= 10;
                    i++;
                }
            } while (n);
            if (sign) {
                WriteData(2 + (6 - i) * 4, 1);
            }
        }
    }

    void ShowChargeLevel(uint8_t charge) {
        WriteData(26, 0x00);
        WriteData(27, 0x00);
        switch (charge) {
            case 0 :
                WriteData(26, 1);
                break;
            case 1 :
                WriteData(26, 1);
                WriteData(27, 2);
                break;
            case 2 :
                WriteData(26, 1);
                WriteData(27, 3);
                break;
            case 3 :
                WriteData(26, 3);
                WriteData(27, 3);
                break;
            default :
                WriteData(26, 0x00);
                WriteData(27, 0x00);
        }
    }
}