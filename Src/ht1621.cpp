#include "ht1621.hpp"

LCD::LCD() : m_cs_pin(GPIOB, 5),
             m_write_pin(GPIOB, 4),
             m_data_pin(GPIOB, 3) {
    m_cs_pin.Init(GpioDriver::Mode::Output,
                  GpioDriver::OutType::PushPull,
                  GpioDriver::Pull::None,
                  GpioDriver::Speed::High);
    m_write_pin.Init(GpioDriver::Mode::Output,
                     GpioDriver::OutType::PushPull,
                     GpioDriver::Pull::None,
                     GpioDriver::Speed::High);
    m_data_pin.Init(GpioDriver::Mode::Output,
                    GpioDriver::OutType::PushPull,
                    GpioDriver::Pull::None,
                    GpioDriver::Speed::High);
}

void LCD::WriteBit(uint8_t bit) {
    uint8_t i;
    if (bit)
        m_data_pin.Set();
    else
        m_data_pin.Reset();
    m_write_pin.Set();
    m_write_pin.Reset();
    for (i = 0; i <= 10; i++);
    m_write_pin.Set();
}

void LCD::WriteCommand(Commands cmd) {
    uint8_t cmd_v = cmd;
    m_cs_pin.Set();
    m_cs_pin.Reset();
    WriteBit(1);
    WriteBit(0);
    WriteBit(0);
    for (uint8_t i = 0; i < 8; i++) {
        if ((cmd_v & 0x80) == 0x80)
            WriteBit(1);
        else
            WriteBit(0);
        cmd_v <<= 1;
    }
    WriteBit(0);
    m_data_pin.Set();
    m_cs_pin.Set();
}

void LCD::WriteData(uint8_t address, uint8_t data) {
    m_cs_pin.Set();
    m_cs_pin.Reset();
    WriteBit(1);
    WriteBit(0);
    WriteBit(1);
    address <<= 2;
    for (uint8_t i = 0; i < 6; i++) {
        if ((address & 0x80) == 0x80)
            WriteBit(1);
        else
            WriteBit(0);
        address <<= 1;
    }
    for (uint8_t i = 0; i < 4; i++) {
        if ((data & 0x01) == 0x01) {
            WriteBit(1);
        } else {
            WriteBit(0);
        }
        data >>= 1;
    }
    m_data_pin.Set();
    m_cs_pin.Set();
}

void LCD::WriteFull() {
    for (uint8_t j = 0; j < 32; j++) {
        WriteData(j, Numbers[j]);
    }
}

void LCD::Clean() {
    for (uint8_t j = 0; j < 28; j++) {
        WriteData(j, 0x00);
    }
}

void LCD::Init() {
    WriteCommand(Commands::Bias05);
    WriteCommand(Commands::SysEn);
    WriteCommand(Commands::LcdOn);
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

void LCD::ShowDigit(uint8_t position, uint8_t digit) {
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

void LCD::ShowDigitDot(uint8_t position, uint8_t digit) {
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

void LCD::ShowInt(int num) {
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

void LCD::ShowDouble(double m) {
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

void LCD::ShowChargeLevel(uint8_t charge) {
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