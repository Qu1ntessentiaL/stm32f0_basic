#include "ht1621.hpp"
#include "config.h"

namespace {
    /** Минимальная длительность такта DATA/WR по datasheet HT1621 (мкс). */
    constexpr uint32_t kBitDelayUs = 1;

    /**
     * @brief Настроить TIM16 как свободно бегущий счётчик 1 МГц (1 тик = 1 мкс).
     *
     * TIM16 больше не используется ни одним другим драйвером проекта, поэтому
     * безопасно занять его целиком под источник точных микросекундных задержек.
     * В отличие от подсчёта циклов CPU (__NOP() в цикле), это не зависит от
     * уровня оптимизации сборки — Debug/Release/MinSizeRel дают одинаковый тайминг.
     */
    void initMicrosecondTimer() {
        RCC->APB2ENR |= RCC_APB2ENR_TIM16EN;
        TIM16->CR1 = 0;
        TIM16->PSC = static_cast<uint16_t>(SYSTEM_CLOCK_HZ / 1'000'000u - 1u);
        TIM16->ARR = 0xFFFFu;
        TIM16->EGR = TIM_EGR_UG;
        TIM16->CNT = 0;
        TIM16->CR1 = TIM_CR1_CEN;
    }

    enum Command : uint8_t {
        SysEn = 0x01,
        LcdOn = 0x03,
        RC256K = 0x18,
        Bias12 = 0x28, // 1/2 bias, 4 commons
    };

    /** Значения 4 нибблов разряда (относительные адреса base+1..base+4). */
    constexpr uint8_t kDigitGlyphs[10][4] = {
        {3, 2, 1, 3}, // 0
        {0, 0, 1, 2}, // 1
        {2, 3, 0, 3}, // 2
        {0, 3, 1, 3}, // 3
        {1, 1, 1, 2}, // 4
        {1, 3, 1, 1}, // 5
        {3, 3, 1, 1}, // 6
        {0, 0, 1, 3}, // 7
        {3, 3, 1, 3}, // 8
        {1, 3, 1, 3}, // 9
    };

    constexpr uint8_t kLetterGlyphs[21][4] = {
        {3, 1, 1, 3}, // A
        {3, 3, 1, 0}, // b
        {3, 2, 0, 1}, // C
        {2, 3, 1, 2}, // d
        {3, 3, 0, 1}, // E
        {3, 1, 0, 1}, // F
        {3, 2, 1, 1}, // G
        {3, 1, 1, 0}, // h
        {3, 0, 0, 0}, // I
        {0, 2, 1, 2}, // J
        {3, 2, 0, 0}, // L
        {2, 1, 1, 0}, // n
        {2, 3, 1, 0}, // o
        {3, 1, 0, 3}, // P
        {2, 1, 0, 0}, // r
        {3, 3, 0, 0}, // t
        {3, 2, 1, 2}, // U
        {3, 1, 1, 2}, // X
        {0, 1, 0, 0}, // -
        {0, 2, 0, 0}, // _
        {0, 0, 0, 0}, // space
    };

    constexpr uint8_t kGlyphDash = 18;
    constexpr uint8_t kSharedNibbleAddr = 0x17;
    constexpr uint8_t kSharedSegMask = 0x01;
    constexpr uint32_t kInitDelayUs = 1000;

    constexpr uint8_t kDotAddrs[5] = {0x03, 0x07, 0x0B, 0x0F, 0x13};
    constexpr uint8_t kDotMask = 0x02;

    constexpr struct {
        uint8_t addr;
        uint8_t mask;
    } kChargeSegs[4] = {
        {0x1A, 1}, {0x1B, 2}, {0x1B, 1}, {0x1A, 2},
    };

    constexpr struct {
        uint8_t addr;
        uint8_t mask;
    } kSpecials[4] = {
        {0x00, 0x01}, {0x00, 0x02}, {0x17, 0x02}, {0x19, 0x02},
    };

    constexpr uint8_t kSpecialsCount = sizeof(kSpecials) / sizeof(kSpecials[0]);

    /** Маска сегментов: addr+3 делит ниббл с DP / иконкой "k". */
    constexpr uint8_t kGlyphMask(uint8_t rel) {
        return (rel == 3) ? 0x01u : 0x03u;
    }
} // namespace

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

    m_cs_pin.Set();
    m_write_pin.Set();
    m_data_pin.Reset();
}

void HT1621B::delayUs(uint32_t us) const {
    const auto start = static_cast<uint16_t>(TIM16->CNT);
    while (static_cast<uint16_t>(static_cast<uint16_t>(TIM16->CNT) - start) < us) {}
}

void HT1621B::beginTransfer(bool isData) const {
    m_cs_pin.Set();
    delayUs(kBitDelayUs);
    m_cs_pin.Reset();
    delayUs(kBitDelayUs);

    writeBit(true);
    writeBit(false);
    writeBit(isData);
}

void HT1621B::endTransfer() const {
    delayUs(kBitDelayUs);
    m_cs_pin.Set();
    delayUs(kBitDelayUs);
}

void HT1621B::writeBit(bool bit) const {
    if (bit)
        m_data_pin.Set();
    else
        m_data_pin.Reset();

    delayUs(kBitDelayUs);

    m_write_pin.Reset();
    delayUs(kBitDelayUs);
    m_write_pin.Set();
    delayUs(kBitDelayUs);
}

void HT1621B::writeCommand(uint8_t cmd) {
    beginTransfer(false);

    for (uint8_t i = 0; i < 8; ++i) {
        writeBit((cmd & 0x80) != 0);
        cmd <<= 1;
    }

    writeBit(false);
    endTransfer();
}

void HT1621B::writeDataBurst(uint8_t startAddr, uint8_t endAddr) {
    if (startAddr >= kVramSize || endAddr >= kVramSize || startAddr > endAddr)
        return;

    beginTransfer(true);

    uint8_t addr = static_cast<uint8_t>(startAddr << 2);
    for (uint8_t i = 0; i < 6; ++i) {
        writeBit((addr & 0x80) != 0);
        addr <<= 1;
    }

    for (uint8_t a = startAddr; a <= endAddr; ++a) {
        uint8_t data = m_vram[a];
        for (uint8_t b = 0; b < 4; ++b) {
            writeBit((data & 0x01) != 0);
            data >>= 1;
        }
    }

    endTransfer();
}

void HT1621B::flushAll() {
    writeDataBurst(0, kVramSize - 1);
}

void HT1621B::flushDirty() {
    uint32_t dirty = m_dirty;
    if (!dirty) return;

    if (dirty == 0xFFFFFFFFu || __builtin_popcount(dirty) > kVramSize / 2) {
        flushAll();
        m_dirty = 0;
        return;
    }

    while (dirty) {
        const uint8_t start = static_cast<uint8_t>(__builtin_ctz(dirty));
        uint8_t end = start;
        uint32_t run = 1u << start;

        while (end + 1 < kVramSize && (dirty & (run << 1))) {
            ++end;
            run <<= 1;
        }

        writeDataBurst(start, end);
        const uint32_t throughEnd = (end == 31) ? UINT32_MAX : ((1u << (end + 1)) - 1u);
        const uint32_t beforeStart = (start == 0) ? 0u : ((1u << start) - 1u);
        dirty &= ~(throughEnd & ~beforeStart);
    }

    m_dirty = 0;
}

void HT1621B::touch(uint8_t addr, uint8_t value) {
    if (addr >= kVramSize) return;
    value &= 0x0Fu;
    if (m_vram[addr] == value) return;
    m_vram[addr] = value;
    m_dirty |= (1u << addr);
}

void HT1621B::setBits(uint8_t addr, uint8_t mask) {
    if (addr >= kVramSize) return;
    const uint8_t next = static_cast<uint8_t>(m_vram[addr] | mask);
    touch(addr, next);
}

void HT1621B::clearBits(uint8_t addr, uint8_t mask) {
    if (addr >= kVramSize) return;
    const uint8_t next = static_cast<uint8_t>(m_vram[addr] & ~mask);
    touch(addr, next);
}

void HT1621B::writeGlyph(uint8_t base, const uint8_t segs[kSegsPerDigit]) {
    for (uint8_t i = 0; i < kSegsPerDigit; ++i) {
        const uint8_t addr = static_cast<uint8_t>(base + i + 1);
        const uint8_t mask = kGlyphMask(static_cast<uint8_t>(i + 1));
        const uint8_t value = static_cast<uint8_t>((m_vram[addr] & ~mask) | (segs[i] & mask));
        touch(addr, value);
    }
}

int HT1621B::letterIndex(char c) {
    switch (c) {
        case 'A': return 0;
        case 'b': return 1;
        case 'C': return 2;
        case 'd': return 3;
        case 'E': return 4;
        case 'F': return 5;
        case 'G': return 6;
        case 'h': return 7;
        case 'I': return 8;
        case 'J': return 9;
        case 'L': return 10;
        case 'n': return 11;
        case 'o': return 12;
        case 'P': return 13;
        case 'r': return 14;
        case 't': return 15;
        case 'U': return 16;
        case 'X': return 17;
        case '-': return 18;
        case '_': return 19;
        case ' ': return 20;
        default: return -1;
    }
}

void HT1621B::showChar(uint8_t position, char c) {
    if (position >= kDigitCount) return;

    if (c >= '0' && c <= '9') {
        writeGlyph(digitBase(position), kDigitGlyphs[c - '0']);
        return;
    }

    const int idx = letterIndex(c);
    if (idx >= 0)
        writeGlyph(digitBase(position), kLetterGlyphs[idx]);
}

void HT1621B::Flush() {
    flushDirty();
}

void HT1621B::FullClear() {
    for (uint8_t i = 0; i < kVramSize; ++i)
        m_vram[i] = 0;
    m_dirty = 0xFFFFFFFFu;
}

void HT1621B::ClearSegArea() {
    for (uint8_t i = 1; i <= kDigitCount * kSegsPerDigit; ++i) {
        if (i == kSharedNibbleAddr)
            clearBits(i, kSharedSegMask);
        else
            touch(i, 0);
    }
}

void HT1621B::Init() {
    initMicrosecondTimer();

    writeCommand(RC256K);
    delayUs(kInitDelayUs);
    writeCommand(Bias12);
    delayUs(kInitDelayUs);
    writeCommand(SysEn);
    delayUs(kInitDelayUs);
    writeCommand(LcdOn);
    delayUs(kInitDelayUs);
    FullClear();
    Flush();
}

void HT1621B::ShowDot(uint8_t position, bool enable) {
    if (position == 0 || position >= kDigitCount) return;

    const uint8_t addr = kDotAddrs[5 - position];
    if (enable)
        setBits(addr, kDotMask);
    else
        clearBits(addr, kDotMask);
}

void HT1621B::ShowSpecial(Special type, bool enable) {
    const uint8_t idx = static_cast<uint8_t>(type);
    if (idx >= kSpecialsCount) return;

    const auto &s = kSpecials[idx];
    if (enable)
        setBits(s.addr, s.mask);
    else
        clearBits(s.addr, s.mask);
}

void HT1621B::ShowDigit(uint8_t position, uint8_t digit, bool withDot) {
    if (position >= kDigitCount || digit > 9) return;

    writeGlyph(digitBase(position), kDigitGlyphs[digit]);

    if (withDot && position > 0)
        setBits(kDotAddrs[5 - position], kDotMask);
}

void HT1621B::ShowFull() {
    for (uint8_t i = 0; i < kVramSize; ++i)
        touch(i, 0x0F);
}

void HT1621B::ShowLetter(uint8_t position, char c) {
    showChar(position, c);
}

void HT1621B::ShowString(const char *str) {
    if (!str) return;

    ClearSegArea();

    uint8_t len = 0;
    while (len < kDigitCount && str[len]) ++len;

    for (uint8_t i = 0; i < len; ++i)
        showChar(i, str[len - 1 - i]);
}

void HT1621B::ShowInt(int value) {
    ClearSegArea();

    const bool negative = value < 0;
    unsigned mag = negative
                       ? static_cast<unsigned>(-(value + 1)) + 1u
                       : static_cast<unsigned>(value);

    unsigned tmp = mag;
    uint8_t digits = 0;
    do {
        ++digits;
        tmp /= 10;
    }
    while (tmp);

    const uint8_t maxDigits = negative ? 5u : 6u;
    if (digits > maxDigits) {
        for (uint8_t i = 0; i < kDigitCount; ++i)
            writeGlyph(digitBase(i), kLetterGlyphs[kGlyphDash]);
        return;
    }

    uint8_t pos = 0;
    do {
        writeGlyph(digitBase(pos), kDigitGlyphs[mag % 10]);
        mag /= 10;
        ++pos;
    }
    while (mag && pos < kDigitCount);

    if (negative && pos < kDigitCount)
        writeGlyph(digitBase(pos), kLetterGlyphs[kGlyphDash]);
}

void HT1621B::ShowChargeLevel(uint8_t level) {
    if (level > 3) level = 3;

    for (uint8_t i = 0; i < 4; ++i) {
        const auto &s = kChargeSegs[i];
        if (i <= level)
            setBits(s.addr, s.mask);
        else
            clearBits(s.addr, s.mask);
    }
}

void HT1621B::ShowDate(uint8_t day, uint8_t month, uint8_t year) {
    day %= 100;
    month %= 100;
    year %= 100;

    if (day == 0 || day > 31 || month == 0 || month > 12)
        return;

    ClearSegArea();

    const uint8_t digits[6] = {
        static_cast<uint8_t>(year % 10),
        static_cast<uint8_t>(year / 10),
        static_cast<uint8_t>(month % 10),
        static_cast<uint8_t>(month / 10),
        static_cast<uint8_t>(day % 10),
        static_cast<uint8_t>(day / 10),
    };

    for (uint8_t pos = 0; pos < kDigitCount; ++pos) {
        writeGlyph(digitBase(pos), kDigitGlyphs[digits[pos]]);
        if (pos == 2 || pos == 4)
            setBits(kDotAddrs[5 - pos], kDotMask);
    }
}
