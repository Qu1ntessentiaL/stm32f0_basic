/**
 * @file ds18x20_scan.cpp
 * @brief Bit-bang Search ROM (Maxim AN187) и печать ROM в UART.
 *
 * @details
 * На время скана PA8 переводится в GPIO open-drain, TIM1 используется только
 * как источник микросекундных пауз (busy-wait по UIF). Алгоритм обхода —
 * классический last-discrepancy: на каждом из 64 бит читаются бит и его
 * дополнение, затем мастер пишет выбранное направление.
 */

#include "ds18x20_scan.hpp"
#include "ds18x20_hw.hpp"
#include "config.h"

namespace {
    using ds18x20_hw::kOwPin;
    using ds18x20_hw::kOwPinMask;

    constexpr uint8_t kSearchMax = 8;          ///< Защита от бесконечного обхода.

    /**
     * @brief Блокирующая пауза TIM1 OPM (1 тик = 1 мкс, PSC уже задан в init).
     */
    void delay_us(uint16_t us) {
        if (us == 0) return;
        TIM1->CR1 = 0;
        TIM1->DIER = 0;
        TIM1->CCER = 0;
        TIM1->ARR = us;
        TIM1->RCR = 0;
        TIM1->CNT = 0;
        TIM1->EGR = TIM_EGR_UG;
        ds18x20_hw::clear_uif();
        TIM1->CR1 = TIM_CR1_OPM | TIM_CR1_CEN;
        while (!(TIM1->SR & TIM_SR_UIF)) {}
        ds18x20_hw::clear_uif();
    }

    /** Притянуть 1-Wire к земле. */
    void ow_low() { GPIOA->BRR = kOwPinMask; }
    /** Отпустить линию (подтяжка поднимает её). */
    void ow_release() { GPIOA->BSRR = kOwPinMask; }
    /** Текущий уровень линии (true = high). */
    bool ow_level() { return (GPIOA->IDR & kOwPinMask) != 0; }

    /**
     * @brief Reset-слот и проверка presence (slave тянет линию после ~70 мкс).
     * @return true, если хотя бы одно устройство ответило.
     */
    bool ow_reset() {
        ow_low();
        delay_us(480);
        ow_release();
        delay_us(70);
        const bool presence = !ow_level();
        delay_us(410);
        return presence;
    }

    /**
     * @brief Запись одного бита (слот ~70 мкс).
     */
    void ow_write_bit(bool bit) {
        ow_low();
        if (bit) {
            delay_us(6);
            ow_release();
            delay_us(64);
        } else {
            delay_us(60);
            ow_release();
            delay_us(10);
        }
    }

    /**
     * @brief Чтение одного бита: короткий low, выборка ~15 мкс от начала слота.
     */
    bool ow_read_bit() {
        ow_low();
        delay_us(6);
        ow_release();
        delay_us(9);
        const bool bit = ow_level();
        delay_us(55);
        return bit;
    }

    /**
     * @brief Запись байта, младший бит первый (как требует 1-Wire).
     */
    void ow_write_byte(uint8_t byte) {
        for (uint8_t i = 0; i < 8; ++i) {
            ow_write_bit((byte & (1u << i)) != 0);
        }
    }

    /**
     * @brief Один проход Search ROM: следующий 64-битный адрес.
     * @param[in,out] rom               Предыдущий найденный ROM (для бит до discrepancy).
     * @param[in,out] last_discrepancy  Позиция последнего ветвления (0 = начало).
     * @param[in,out] last_device       true, когда обход закончен.
     * @return true, если в @p rom лежит новый валидный по наличию на шине адрес.
     *
     * @see Maxim Application Note 187
     */
    bool search_next(uint8_t rom[8], int &last_discrepancy, bool &last_device) {
        if (last_device || !ow_reset()) {
            last_discrepancy = 0;
            last_device = true;
            return false;
        }

        ow_write_byte(0xF0);

        int id_bit_number = 1;
        int last_zero = 0;
        uint8_t rom_byte = 0;
        uint8_t rom_mask = 1;

        do {
            const bool id_bit = ow_read_bit();
            const bool cmp_bit = ow_read_bit();
            bool direction;

            if (id_bit && cmp_bit) {
                last_discrepancy = 0;
                last_device = true;
                return false;
            }

            if (id_bit != cmp_bit) {
                direction = id_bit;
            } else {
                direction = (id_bit_number < last_discrepancy)
                            ? ((rom[rom_byte] & rom_mask) != 0)
                            : (id_bit_number == last_discrepancy);
                if (!direction) last_zero = id_bit_number;
            }

            if (direction) rom[rom_byte] |= rom_mask;
            else rom[rom_byte] &= static_cast<uint8_t>(~rom_mask);

            ow_write_bit(direction);
            ++id_bit_number;
            rom_mask = static_cast<uint8_t>(rom_mask << 1);
            if (rom_mask == 0) {
                ++rom_byte;
                rom_mask = 1;
            }
        } while (rom_byte < 8);

        last_discrepancy = last_zero;
        last_device = (last_discrepancy == 0);
        return true;
    }

    /**
     * @brief Два hex-символа без завершающего нуля в динамической куче (стек, 3 байта).
     */
    void uart_hex_byte(UsartDriver<> *uart, uint8_t value) {
        constexpr char kHex[] = "0123456789ABCDEF";
        const char buf[3] = {kHex[value >> 4], kHex[value & 0x0F], '\0'};
        uart->write_str(buf);
    }

    /**
     * @brief Человекочитаемое имя по family code ROM[0].
     */
    const char *family_name(uint8_t family) {
        if (family == 0x28) return "DS18B20";
        if (family == 0x10) return "DS18S20";
        return "unknown";
    }

    /**
     * @brief Остановить TIM1 как ШИМ и перевести PA8 в GPIO OD.
     */
    void begin_bitbang() {
        TIM1->CR1 = 0;
        TIM1->CCER = 0;
        TIM1->DIER = 0;
        GPIOA->MODER = (GPIOA->MODER & ~(3u << (kOwPin * 2))) | (1u << (kOwPin * 2));
        GPIOA->OTYPER |= kOwPinMask;
        GPIOA->OSPEEDR |= (3u << (kOwPin * 2));
        GPIOA->PUPDR &= ~(3u << (kOwPin * 2));
        ow_release();
    }
}

void ds18x20_scan_bus(UsartDriver<> *uart) {
    if constexpr (!DS18X20_BUS_SCAN_ENABLED) {
        return;
    }
    if (!uart) return;

    begin_bitbang();
    uart->write_str("DS18x20 bus scan:\r\n");

    uint8_t rom[8] = {};
    int last_discrepancy = 0;
    bool last_device = false;
    uint8_t found = 0;

    while (found < kSearchMax && search_next(rom, last_discrepancy, last_device)) {
        uart->write_str("  [");
        uart->write_int(found);
        uart->write_str("] ");
        uart->write_str(family_name(rom[0]));
        uart->write_str("  {{");
        for (uint8_t i = 0; i < 8; ++i) {
            uart->write_str("0x");
            uart_hex_byte(uart, rom[i]);
            if (i + 1 < 8) uart->write_str(", ");
        }
        uart->write_str("}}");
        if (ds18x20_hw::dallas_crc8(rom, 7) != rom[7]) {
            uart->write_str("  CRC error");
        }
        uart->write_str("\r\n");
        uart->flush();
        ++found;
    }

    if (found == 0) {
        uart->write_str("  no devices\r\n");
    } else if (!last_device && found == kSearchMax) {
        uart->write_str("  (scan truncated)\r\n");
    }
    uart->write_str("Found: ");
    uart->write_int(found);
    uart->write_str("\r\n");
    uart->flush();
}
