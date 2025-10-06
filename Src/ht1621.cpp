#include "ht1621.hpp"

static inline void itoa_simple(int value, char *buf) {
    char tmp[12];
    int i = 0;
    bool neg = false;

    if (value < 0) {
        neg = true;
        value = -value;
    }

    do {
        tmp[i++] = '0' + (value % 10);
        value /= 10;
    } while (value);

    if (neg)
        *buf++ = '-';

    while (i--)
        *buf++ = tmp[i];

    *buf = '\0';
}

static inline void ftoa_simple(double val, char *buf, uint8_t decimals) {
    if (val < 0) {
        *buf++ = '-';
        val = -val;
    }

    int int_part = (int) val;
    double frac = val - int_part;

    // целая часть
    char intbuf[12];
    itoa_simple(int_part, intbuf);
    for (char *p = intbuf; *p; ++p) *buf++ = *p;

    *buf++ = '.';

    // дробная часть
    for (uint8_t i = 0; i < decimals; ++i) {
        frac *= 10;
        int digit = (int) frac;
        *buf++ = '0' + digit;
        frac -= digit;
    }

    *buf = '\0';
}


HT1621B::HT1621B() : m_cs_pin(GPIOB, 5),
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

void HT1621B::WriteBit(uint8_t bit) {
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

void HT1621B::WriteCommand(Commands cmd) {
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

void HT1621B::WriteData(uint8_t address, uint8_t data) {
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

void HT1621B::WriteFull() {
    for (uint8_t j = 0; j < 32; j++) {
        WriteData(j, Numbers[j]);
    }
}

void HT1621B::Flush() {
    for (uint8_t i = 0; i < 32; i++) {
        WriteData(i, m_vram[i]);
    }
}

void HT1621B::Clear() {
    memset(m_vram, 0, sizeof(m_vram));
    Flush();
}

void HT1621B::Init() {
    WriteCommand(Commands::Bias05);
    WriteCommand(Commands::SysEn);
    WriteCommand(Commands::LcdOn);
    Clear();
}

void HT1621B::ShowDigit(uint8_t position, uint8_t digit, bool withDot) {
    if (position >= 6 || digit > 9) return;

    const uint8_t base = (5 - position) * 4;

    // Отрисовка цифры
    for (const auto &seg: digits[digit]) {
        if (seg.addr == 0) break;
        WriteData(seg.addr + base, seg.val);
    }

    // Добавляем точку, если нужно
    if (withDot) {
        if (position == 0) return;
        const auto &dot = dots[5 - position];
        WriteData(dot.addr, dot.val);
    }
}

void HT1621B::ShowInt(int value) {
    char buf[8];
    itoa_simple(value, buf);
    Clear();

    bool negative = (value < 0);
    uint8_t start = negative ? 1 : 0; // Пропустить '-' при выводе цифр

    // Вычисляем длину (без знака)
    uint8_t len = 0;
    while (buf[len]) len++;
    uint8_t digits = len - start;

    // Проверяем, влезет ли в 6 позиций
    // Если отрицательное — минус + 5 цифр, иначе 6 цифр максимум
    if ((!negative && digits > 6) || (negative && digits > 5)) {
        // Покажем "------" как индикатор переполнения
        for (uint8_t i = 0; i < 6; ++i)
            WriteData(i * 4 + 2, 1); // Короткий сегмент по центру
        return;
    }

    uint8_t pos = 0;
    for (int i = len - 1; i >= start && pos < 6; --i) {
        ShowDigit(pos++, buf[i] - '0');
    }

    // Если отрицательное и есть место — показываем минус
    if (negative && pos < 6) {
        // Нарисуем "–" слева от числа
        // Можно использовать WriteData(base + смещение, значение)
        uint8_t base = (5 - pos) * 4;
        WriteData(base + 2, 1); // Маленький горизонтальный сегмент
    }
}

void HT1621B::ShowDouble(double value, uint8_t decimals) {
    char buf[16];
    ftoa_simple(value, buf, decimals);
    Clear();

    bool negative = (value < 0);

    // Вычисляем длину строки
    uint8_t len = 0;
    for (; buf[len]; ++len);

    // Считаем количество цифр (без точки и минуса)
    uint8_t digits = 0;
    for (uint8_t i = 0; i < len; ++i)
        if (buf[i] >= '0' && buf[i] <= '9')
            digits++;

    // Проверяем, влезает ли всё в 6 индикаторов
    // Минус и точка считаются как отдельные позиции
    uint8_t needed = digits + (negative ? 1 : 0);
    if (needed > 6) {
        // Показать "------" при переполнении
        for (uint8_t i = 0; i < 6; ++i)
            WriteData(i * 4 + 2, 1);
        return;
    }

    // Начинаем вывод справа налево
    uint8_t pos = 0;
    for (int i = len - 1; i >= 0 && pos < 6; --i) {
        if (buf[i] == '.' || buf[i] == '-') continue;

        bool dot = (i < len - 1 && buf[i + 1] == '.');
        ShowDigit(pos++, buf[i] - '0', dot);
    }

    // Если отрицательное и ещё есть место — показываем минус
    if (negative && pos < 6) {
        uint8_t base = (5 - pos) * 4;
        WriteData(base + 2, 1); // Горизонтальный сегмент «–»
    }
}

void HT1621B::ShowChargeLevel(uint8_t level) {
    if (level > 3) level = 0;
    WriteData(26, chargeLevels[level][0]);
    WriteData(27, chargeLevels[level][1]);
}