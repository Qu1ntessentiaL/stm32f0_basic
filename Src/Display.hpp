#pragma once

#include "GpioDriver.hpp"

// Commands for HT1621B
#define BIAS05 0x21 // Set 1/2 bias, 4 commons
#define SYSEN  0x01 // System enable
#define LCDON  0x03 // Turn on LCD

class Display {
    GpioDriver *m_cs;
    GpioDriver *m_wr;
    GpioDriver *m_data;

    uint8_t m_displayRam[32] = {0}; // Each address is 4-bit

    /**
     * @brief Generate a small delay (few CPU cycles)
     */
    static inline void smallDelay() {
        for (uint8_t i = 0; i < 10; i++) {
            __NOP();
        }
    }

    /**
     * @brief Write a single bit to the HT1621B. Data must be set before toggling WR.
     * @param bit
     */
    inline void writeBit(bool bit) {
        bit ? m_data->Set() : m_data->Reset();
        smallDelay();

        // Latch on rising edge of WR
        m_wr->Reset();
        smallDelay();
        m_wr->Set();
        smallDelay();
    }

    /**
     * @brief Send a command byte to the HT1621B (CMD mode)
     * @param cmd
     */
    inline void writeCommand(uint8_t cmd) {
        m_cs->Set();
        m_cs->Reset();
        smallDelay();

        // Start bits: ST2, ST1, ST0 = 1, 0, 0
        writeBit(true);
        writeBit(false);
        writeBit(false);

        // Mode bits: C1, C0 = 0, 0 for command
        writeBit(false);
        writeBit(false);

        // Send 8-bit command MSB first
        for (uint8_t i = 7; i >= 0; --i) {
            writeBit((cmd >> i) & 0x01);
        }

        // Stop bit
        writeBit(false);

        // End transaction
        m_cs->Set();
        smallDelay();
    }

    /**
     * @brief Write one 4-bit value to a RAM address
     * @param address
     * @param data [4-bit]
     */
    inline void writeData(uint8_t address, uint8_t data) {
        m_cs->Reset();

        // Start bits: 1 0 1 (write data)
        writeBit(true);
        writeBit(false);
        writeBit(true);

        // Send 6-bit address (MSB first), HT1621 expects only upper 6 bits of (address << 2)
        uint8_t addr = address << 2;
        for (uint8_t i = 0; i < 6; ++i) {
            writeBit((addr & 0x80) != 0);
            addr << 1;
        }

        // Send 4-bit data (LSB first)
        for (uint8_t i = 0; i < 4; ++i) {
            writeBit((data >> i) & 0x01);
        }

        m_cs->Set();
    }

public:
    explicit Display(GpioDriver *cs, GpioDriver *wr, GpioDriver *data)
            : m_cs(cs), m_wr(wr), m_data(data) {}

    /**
     * @brief Initialize GPIOs and HT1621B
     */
    inline void Init() {
        m_cs->Init(GpioDriver::Mode::Output, GpioDriver::OutType::PushPull,
                   GpioDriver::Pull::None, GpioDriver::Speed::Medium);
        m_wr->Init(GpioDriver::Mode::Output, GpioDriver::OutType::PushPull,
                   GpioDriver::Pull::None, GpioDriver::Speed::Medium);
        m_data->Init(GpioDriver::Mode::Output, GpioDriver::OutType::PushPull,
                     GpioDriver::Pull::None, GpioDriver::Speed::Medium);

        // Ensure lines are idle high
        m_cs->Set();
        m_wr->Set();
        m_data->Set();

        // Send basic commands
        writeCommand(BIAS05);
        writeCommand(SYSEN);
        writeCommand(LCDON);

        // Clear display RAM
        Clean();
    }

    /**
     * @brief Clear all RAM locations (assumes 32 RAM addresses)
     */
    inline void Clean() {
        // Enter write mode: ST = 1, 0, 1; RW = "write" (1); auto-increment (1)
        m_cs->Set();
        m_cs->Reset();
        smallDelay();

        // Start bits: 1, 0, 1
        writeBit(true);
        writeBit(false);
        writeBit(true);

        // RW bit = 1 (write)
        writeBit(true);

        // Address mode: auto-increment = 1
        writeBit(true);

        // Write zeros to all 32 addresses (each address has 4 bits)
        for (uint8_t addr = 0; addr < 32; ++addr) {
            for (uint8_t bit = 0; bit < 4; ++bit) {
                writeBit(false);
            }
        }

        // End transaction
        m_cs->Set();
        smallDelay();
    }

    /**
     * @brief Set a 4-bit value at a specific address
     * @param address
     * @param value
     */
    inline void SetSymbol(uint8_t address, uint8_t value) {
        if (address < 32) {
            m_displayRam[address] = value & 0x0F;
            writeData(address, value & 0x0F);
        }
    }

    /**
     * Update entire display from RAM buffer
     */
    inline void Refresh() {
        for (int i = 0; i < 32; ++i) {
            writeData(i, m_displayRam[i]);
        }
    }

    /**
     * @brief Fill display with test pattern (all segments ON)
     */
    inline void TestFill() {
        for (int i = 0; i < 32; ++i) {
            m_displayRam[i] = 0x0F; // All 4 bits set
            writeData(i, 0x0F);
        }
    }
};