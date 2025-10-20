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

// Расходует очень много Flash
/*
static inline void ftoa_simple(double val, char *buf, uint8_t decimals) {
    if (val < 0) {
        *buf++ = '-';
        val = -val;
    }

    int int_part = (int) val;
    double frac = val - int_part;

    // Целая часть
    char intbuf[12];
    itoa_simple(int_part, intbuf);
    for (char *p = intbuf; *p; ++p) *buf++ = *p;

    *buf++ = '.';

    // Дробная часть
    for (uint8_t i = 0; i < decimals; ++i) {
        frac *= 10;
        int digit = (int) frac;
        *buf++ = '0' + digit;
        frac -= digit;
    }

    *buf = '\0';
}

static inline void ftoa_simple_f(float val, char *buf, uint8_t decimals) {
    if (val < 0) {
        *buf++ = '-';
        val = -val;
    }

    auto int_part = (uint32_t) val;
    float frac = val - (float) int_part;

    // Целая часть
    char intbuf[12];
    itoa_simple(int_part, intbuf);
    for (char *p = intbuf; *p; ++p) *buf++ = *p;

    if (decimals > 0) {
        *buf++ = '.';

        // Дробная часть
        for (uint8_t i = 0; i < decimals; ++i) {
            frac *= 10.0f;
            auto d = (uint8_t) frac;
            *buf++ = '0' + d;
            frac -= d;
        }
    }

    *buf = '\0';
}
*/

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
    Init();
}

/**
 * @defgroup Низкоуровневые функции работы с HT1621B
 */

void HT1621B::WriteBit(uint8_t bit) {
    if (bit)
        m_data_pin.Set();
    else
        m_data_pin.Reset();

    m_write_pin.Set();
    m_write_pin.Reset();
    __NOP();
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
    if (address >= 32) return;

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
        WriteBit((data & 0x01) ? 1 : 0);
        data >>= 1;
    }

    m_data_pin.Set();
    m_cs_pin.Set();
}

/**
 * @defgroup Функции для работы с VRAM
 */

void HT1621B::SetData(uint8_t address, uint8_t data) {
    if (address < sizeof(m_vram))
        m_vram[address] = data;
}

void HT1621B::Flush() {
    for (size_t i = 0; i < sizeof(m_vram); i++) {
        WriteData(i, m_vram[i]);
    }
}

void HT1621B::Clear(bool flushNow) {
    memset(m_vram, 0, sizeof(m_vram));
    if (flushNow) Flush();
}

void HT1621B::Init() {
    WriteCommand(Commands::Bias05);
    WriteCommand(Commands::SysEn);
    WriteCommand(Commands::LcdOn);
    Clear(true);
}

void HT1621B::ShowDigit(uint8_t position, uint8_t digit, bool withDot, bool flushNow) {
    if (position >= 6 || digit > 9) return;

    const uint8_t base = (5 - position) * 4;

    // Отрисовка цифры
    for (const auto &seg: m_digits[digit]) {
        if (seg.addr == 0) break;
        SetData(seg.addr + base, seg.val);
    }

    // Добавляем точку, если нужно
    if (withDot && position > 0) {
        const auto &dot = m_dots[5 - position];
        SetData(dot.addr, dot.val);
    }

    if (flushNow) Flush();
}

void HT1621B::ShowFull(bool flushNow) {
    memset(m_vram, 0xff, sizeof(m_vram));
    if (flushNow) Flush();
}

void HT1621B::ShowLetter(uint8_t position, char c, bool flushNow) {
    if (position >= 6) return;

    const uint8_t base = (5 - position) * 4;

    const Segment *segs = nullptr;
    uint8_t segCount = 0;

    switch (c) {
        case 'A': segs = m_letters[0]; segCount = 5; break;
        case 'b': segs = m_letters[1]; segCount = 4; break;
        case 'C': segs = m_letters[2]; segCount = 3; break;
        case 'd': segs = m_letters[3]; segCount = 4; break;
        case 'E': segs = m_letters[4]; segCount = 4; break;
        case 'F': segs = m_letters[5]; segCount = 3; break;
        case 'G': segs = m_letters[6]; segCount = 4; break;
        case 'h': segs = m_letters[7]; segCount = 4; break;
        case 'I': segs = m_letters[8]; segCount = 2; break;
        case 'J': segs = m_letters[9]; segCount = 3; break;
        case 'L': segs = m_letters[10]; segCount = 2; break;
        case 'n': segs = m_letters[11]; segCount = 3; break;
        case 'o': segs = m_letters[12]; segCount = 3; break;
        case 'P': segs = m_letters[13]; segCount = 4; break;
        case 'r': segs = m_letters[14]; segCount = 2; break;
        case 'S': segs = m_letters[15]; segCount = 4; break;
        case 't': segs = m_letters[16]; segCount = 3; break;
        case 'U': segs = m_letters[17]; segCount = 4; break;
        case '-': segs = m_letters[19]; segCount = 1; break;
        case '_': segs = m_letters[20]; segCount = 1; break;
        default: return;
    }

    for (uint8_t i = 0; i < segCount; ++i) {
        SetData(segs[i].addr + base, segs[i].val);
    }

    if (flushNow) Flush();
}

void HT1621B::ShowInt(int value, bool flushNow) {
    char buf[8];
    itoa_simple(value, buf);

    bool negative = (value < 0);
    uint8_t start = negative ? 1 : 0; // Пропустить '-' при выводе цифр

    // Вычисляем длину (без знака)
    uint8_t len = 0;
    while (buf[len]) len++;
    uint8_t l_digits = len - start;

    // Проверяем, влезет ли в 6 позиций
    // Если отрицательное — минус + 5 цифр, иначе 6 цифр максимум
    if ((!negative && l_digits > 6) || (negative && l_digits > 5)) {
        // Покажем "------" как индикатор переполнения
        for (uint8_t i = 0; i < 6; ++i)
            SetData(i * 4 + 2, 1); // Короткий сегмент по центру
        if (flushNow) Flush();
        return;
    }

    uint8_t pos = 0;
    for (int i = len - 1; i >= start && pos < 6; --i) {
        ShowDigit(pos++, buf[i] - '0', false, false);
    }

    // Если отрицательное и есть место — показываем минус
    if (negative && pos < 6) {
        // Нарисуем "–" слева от числа
        // Можно использовать WriteData(base + смещение, значение)
        uint8_t base = (5 - pos) * 4;
        SetData(base + 2, 1); // Маленький горизонтальный сегмент
    }

    if (flushNow) Flush();
}

void HT1621B::ShowChargeLevel(uint8_t level, bool flushNow) {
    if (level > 3) level = 0;
    SetData(26, m_chargeLevels[level][0]);
    SetData(27, m_chargeLevels[level][1]);
    if (flushNow) Flush();
}

void HT1621B::ShowDate(uint8_t day, uint8_t month, uint8_t year, bool flushNow) {
    // Обрезаем значения
    day   %= 100;
    month %= 100;
    year  %= 100;

    if (day == 0 || day > 31 ||
        month == 0 || month > 12) {
        return;
    }

    Clear(false);

    // Формируем цифры в порядке, соответствующем ShowDigit(pos,...)
    // pos 0 = правый символ (год единицы), pos 5 = левый (десятки дня)
    uint8_t digits[6];
    digits[0] = year % 10;
    digits[1] = year / 10;
    digits[2] = month % 10;
    digits[3] = month / 10;
    digits[4] = day % 10;
    digits[5] = day / 10;

    // Выводим справа налево: позиция 0 — правый сегмент, позиция 5 — левый
    // Формат: DD.MM.YY
    for (uint8_t pos = 0; pos < 6; ++pos) {
        bool dot = false;
        if (pos == 2 || pos == 4) dot = true; // точки между D и M, между M и Y
        ShowDigit(pos, digits[pos], dot, false);
    }

    if (flushNow) Flush();
}

// Расходует очень много Flash
/*
void HT1621B::ShowDouble(double value, uint8_t decimals, bool flushNow) {
    char buf[16];
    ftoa_simple(value, buf, decimals);

    bool negative = (value < 0);

    // Вычисляем длину строки
    uint8_t len = 0;
    for (; buf[len]; ++len);

    // Считаем количество цифр (без точки и минуса)
    uint8_t l_digits = 0;
    for (uint8_t i = 0; i < len; ++i)
        if (buf[i] >= '0' && buf[i] <= '9')
            l_digits++;

    // Проверяем, влезает ли всё в 6 индикаторов
    // Минус и точка считаются как отдельные позиции
    uint8_t needed = l_digits + (negative ? 1 : 0);
    if (needed > 6) {
        // Показать "------" при переполнении
        for (uint8_t i = 0; i < 6; ++i)
            SetData(i * 4 + 2, 1);
        if (flushNow) Flush();
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
        SetData(base + 2, 1); // Горизонтальный сегмент «–»
    }

    if (flushNow) Flush();
}

void HT1621B::ShowFloat(float value, uint8_t decimals, bool flushNow) {
    char buf[16];
    ftoa_simple_f(value, buf, decimals);

    bool negative = (value < 0);

    // Считаем символы
    uint8_t len = 0;
    for (; buf[len]; ++len);

    // Подсчёт цифр (без точки и минуса)
    uint8_t l_digits = 0;
    for (uint8_t i = 0; i < len; ++i)
        if (buf[i] >= '0' && buf[i] <= '9')
            l_digits++;

    // Проверка переполнения: минус + цифры ≤ 6
    uint8_t needed = l_digits + (negative ? 1 : 0);
    if (needed > 6) {
        // Показать "------"
        for (uint8_t i = 0; i < 6; ++i)
            SetData(i * 4 + 2, 1);
        if (flushNow) Flush();
        return;
    }

    // Вывод справа налево
    uint8_t pos = 0;
    for (int i = len - 1; i >= 0 && pos < 6; --i) {
        if (buf[i] == '.' || buf[i] == '-') continue;

        bool dot = (i < len - 1 && buf[i + 1] == '.');
        ShowDigit(pos++, buf[i] - '0', dot, false);
    }

    // Если отрицательное и есть место
    if (negative && pos < 6) {
        uint8_t base = (5 - pos) * 4;
        SetData(base + 2, 1); // Минус
    }

    if (flushNow) Flush();
}
*/