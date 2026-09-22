/**
 * @file ds18x20.cpp
 * @brief Реализация неблокирующего FSM DS18X20 (TIM1 + DMA).
 *
 * @details
 * Импульсы команд собираются в compile-time массивы (@c kConvertCmd,
 * @c kSkipReadCmd, @c kMatchReadCmds) и лежат во flash: CPU только
 * подставляет указатель в DMA. Рабочий путь не содержит микросекундных
 * busy-wait — ожидание завершения всегда через TIM1 UIF в @ref DS18X20::poll().
 */

#include "ds18x20.hpp"

#include <array>
#include <utility>

namespace {
    using ds18x20_hw::kOwPin;
    using ds18x20_hw::kOwPinMask;
    using ds18x20_hw::kTimPrescaler;
    using ds18x20_hw::kResetMinUs;
    using ds18x20_hw::kResetMaxUs;
    using ds18x20_hw::kPresenceMinUs;
    using ds18x20_hw::kPresenceMaxUs;

    constexpr uint8_t kShortPulseMax = 0x0A; ///< ≤10 мкс в read-слоте = логическая «1».
    constexpr uint8_t kOnePulse = 1;         ///< Master pull-down для записи «1» / read-слота.
    constexpr uint8_t kZeroPulse = 60;       ///< Master pull-down для записи «0».
    constexpr uint16_t kBitSlot = kOnePulse + kZeroPulse + 1; ///< Период бит-слота, мкс.

    constexpr uint16_t kWaitTickUs = 62500;  ///< Квант длинных пауз TIM1.
    constexpr uint8_t kConvertRepeats = 11;  ///< (11+1)*62500 мкс = 750 мс (Convert T).
    constexpr uint8_t kPauseRepeats = 3;     ///< (3+1)*62500 мкс = 250 мс между циклами.

    /**
     * @brief Длительность master-импульса для одного бита (LSB first).
     * @return @c kOnePulse если бит = 1, иначе @c kZeroPulse.
     */
    constexpr uint8_t bitToPulse(uint8_t byte, uint8_t bit) noexcept {
        return (byte & (1u << bit)) ? kOnePulse : kZeroPulse;
    }

    /**
     * @brief Развернуть байты команды в последовательность длительностей + хвост 0.
     * @note Размер результата @c N*8+1: DMA читает @c bit_count элементов с @c cmd[1].
     */
    template<std::size_t N>
    constexpr auto makeCommand(const std::array<uint8_t, N> &bytes) noexcept {
        std::array<uint8_t, N * 8 + 1> cmd{};
        for (std::size_t i = 0; i < N; ++i) {
            for (std::size_t bit = 0; bit < 8; ++bit) {
                cmd[i * 8 + bit] = bitToPulse(bytes[i], bit);
            }
        }
        return cmd;
    }

    /** Skip ROM + Convert T. */
    constexpr auto kConvertCmd = makeCommand(std::array<uint8_t, 2>{0xCC, 0x44});
    /** Skip ROM + Read Scratchpad (одиночный датчик без прописанного ROM). */
    constexpr auto kSkipReadCmd = makeCommand(std::array<uint8_t, 2>{0xCC, 0xBE});

    /**
     * @brief Байты Match ROM + Read Scratchpad для слота @p Slot.
     */
    template<std::size_t Slot>
    constexpr auto match_bytes() {
        const auto &r = DS18X20_SENSORS[Slot].bytes;
        return std::array<uint8_t, 10>{
                0x55, r[0], r[1], r[2], r[3], r[4], r[5], r[6], r[7], 0xBE
        };
    }

    using MatchCmd = std::array<uint8_t, 10 * 8 + 1>;

    /**
     * @brief Таблица импульсов Match+Read на каждый слот (во flash).
     */
    template<std::size_t... I>
    constexpr std::array<MatchCmd, sizeof...(I)> make_match_cmds(std::index_sequence<I...>) {
        return {makeCommand(match_bytes<I>())...};
    }

    constexpr auto kMatchReadCmds =
            make_match_cmds(std::make_index_sequence<DS18X20_SENSOR_COUNT>{});

    constexpr bool specified_roms_crc_ok() {
        for (const auto &rom : DS18X20_SENSORS) {
            if (!rom.specified()) continue;
            if (ds18x20_hw::dallas_crc8(rom.bytes, 7) != rom.bytes[7]) return false;
        }
        return true;
    }

    static_assert(specified_roms_crc_ok(),
                  "CRC ROM в DS18X20_SENSORS не сходится (байты 0..6 vs байт 7)");
}

bool DS18X20::rom_specified(uint8_t slot) const {
    return slot < DS18X20_SENSOR_COUNT && DS18X20_SENSORS[slot].specified();
}

void DS18X20::configure_measurement_pin() {
    GPIOA->MODER = (GPIOA->MODER & ~(3u << (kOwPin * 2))) | (2u << (kOwPin * 2));
    GPIOA->OTYPER |= kOwPinMask; // open-drain: 1-Wire
    GPIOA->OSPEEDR |= (3u << (kOwPin * 2));
    GPIOA->PUPDR &= ~(3u << (kOwPin * 2));
    GPIOA->AFR[1] = (GPIOA->AFR[1] & ~0xFu) | 0x2u;
}

void DS18X20::force_update() {
    TIM1->EGR = TIM_EGR_UG;
    ds18x20_hw::clear_uif();
}

void DS18X20::start_timer(uint16_t arr, uint8_t rcr) {
    TIM1->ARR = arr;
    TIM1->RCR = rcr;
    force_update();
    TIM1->CR1 = TIM_CR1_OPM | TIM_CR1_CEN;
}

void DS18X20::reset_bus() {
    TIM1->ARR = static_cast<uint16_t>(kResetMinUs * 2);
    TIM1->CCR1 = static_cast<uint16_t>(kResetMinUs);
    TIM1->CCMR1 = (TIM_CCMR1_OC1M_0 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1PE) |
                  (TIM_CCMR1_CC2S_1 | TIM_CCMR1_IC2F_0 | TIM_CCMR1_IC2F_1 | TIM_CCMR1_IC2F_2);
    TIM1->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E;
    TIM1->RCR = 0;
    DMA1_Channel3->CCR = 0;
    DMA1_Channel3->CPAR = reinterpret_cast<uint32_t>(&TIM1->CCR2);
    DMA1_Channel3->CMAR = reinterpret_cast<uint32_t>(m_edge);
    DMA1_Channel3->CNDTR = 2;
    DMA1_Channel3->CCR = DMA_CCR_MINC | DMA_CCR_PSIZE_0 | DMA_CCR_MSIZE_0 | DMA_CCR_EN;
    force_update();
    TIM1->CCR1 = 0;
    TIM1->DIER = TIM_DIER_CC2DE;
    TIM1->CR1 = TIM_CR1_OPM | TIM_CR1_CEN;
}

void DS18X20::send_command(const uint8_t *cmd, uint16_t bit_count) {
    TIM1->RCR = static_cast<uint8_t>(bit_count - 1);
    TIM1->ARR = kBitSlot;
    TIM1->CCR1 = cmd[0];
    TIM1->CCR4 = kOnePulse + kZeroPulse;
    TIM1->CCMR1 = TIM_CCMR1_OC1M_0 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;
    TIM1->CCER = TIM_CCER_CC1E;
    TIM1->DIER = TIM_DIER_CC4DE;
    force_update();
    DMA1_Channel4->CCR = 0;
    DMA1_Channel4->CPAR = reinterpret_cast<uint32_t>(&TIM1->CCR1);
    DMA1_Channel4->CMAR = reinterpret_cast<uint32_t>(&cmd[1]);
    DMA1_Channel4->CNDTR = bit_count;
    DMA1_Channel4->CCR = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PSIZE_0 | DMA_CCR_EN;
    TIM1->CR1 = TIM_CR1_OPM | TIM_CR1_CEN;
}

void DS18X20::read_data() {
    TIM1->RCR = 72 - 1;
    TIM1->ARR = kBitSlot;
    TIM1->CCR1 = kOnePulse;
    TIM1->CCMR1 = (TIM_CCMR1_OC1M_0 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1PE) |
                  (TIM_CCMR1_CC2S_1 | TIM_CCMR1_IC2F_0 | TIM_CCMR1_IC2F_1 | TIM_CCMR1_IC2F_2);
    TIM1->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E;
    TIM1->DIER = TIM_DIER_CC2DE;
    force_update();
    TIM1->CCR1 = 0;
    DMA1_Channel3->CCR = 0;
    DMA1_Channel3->CPAR = reinterpret_cast<uint32_t>(&TIM1->CCR2);
    DMA1_Channel3->CMAR = reinterpret_cast<uint32_t>(m_pulse);
    DMA1_Channel3->CNDTR = 72;
    DMA1_Channel3->CCR = DMA_CCR_MINC | DMA_CCR_EN;
    TIM1->CR1 = TIM_CR1_OPM | TIM_CR1_CEN;
}

bool DS18X20::check_presence() const {
    return (m_edge[0] >= kResetMinUs) && (m_edge[0] <= kResetMaxUs) &&
           (m_edge[1] >= kPresenceMinUs) && (m_edge[1] <= kPresenceMaxUs);
}

void DS18X20::emit(int16_t temp) {
    if (m_slot < DS18X20_SENSOR_COUNT) {
        m_last_temp[m_slot] = temp;
    }
    if (m_on_sample) {
        m_on_sample(m_slot, temp);
    }
}

int16_t DS18X20::decode_temperature(const uint8_t pad[9]) const {
    const auto raw = static_cast<int16_t>((pad[1] << 8) | pad[0]);
    if (m_family == Family::DS18S20) {
        const int16_t cnt_rem = pad[6];
        const int16_t cnt_per_c = pad[7];
        if (cnt_per_c == 0) {
            return TEMP_ERROR_GENERIC;
        }
        const int32_t coarse = (raw >> 1) * 10;
        const int32_t fine = ((cnt_per_c - cnt_rem) * 10) / cnt_per_c;
        return static_cast<int16_t>(coarse - 2 + fine);
    }
    return static_cast<int16_t>((raw * 10) >> 4);
}

void DS18X20::emit_bus_absent() {
    for (uint8_t i = 0; i < DS18X20_SENSOR_COUNT; ++i) {
        m_slot = i;
        emit(TEMP_ERROR_NO_SENSOR);
    }
    m_attempts = 0;
    pause_or_idle();
}

void DS18X20::send_slot_read() {
    if (rom_specified(m_slot)) {
        send_command(kMatchReadCmds[m_slot].data(), kMatchReadBits);
    } else {
        send_command(kSkipReadCmd.data(), kSkipCmdBits);
    }
}

void DS18X20::decode_and_report() {
    uint8_t pad[9]{};
    for (unsigned byte = 0; byte < 9; ++byte) {
        const unsigned bit_start = byte * 8;
        for (unsigned bit = 0; bit < 8; ++bit) {
            if (m_pulse[bit_start + bit] <= kShortPulseMax) {
                pad[byte] = static_cast<uint8_t>(pad[byte] | (1u << bit));
            }
        }
    }

    if (rom_specified(m_slot)) {
        m_family = static_cast<Family>(DS18X20_SENSORS[m_slot].bytes[0]);
    } else {
        m_family = (pad[4] == 0xFF) ? Family::DS18S20 : Family::DS18B20;
    }

    if (pad[8] == ds18x20_hw::dallas_crc8(pad, 8)) {
        m_attempts = 0;
        emit(decode_temperature(pad));
        next_slot_or_idle();
        return;
    }

    ++m_attempts;
    if (m_attempts < SENSOR_MAX_RETRIES) {
        reset_bus();
        m_state = State::SlotSelect;
        return;
    }

    m_attempts = 0;
    emit(TEMP_ERROR_CRC_FAIL);
    next_slot_or_idle();
}

void DS18X20::pause_or_idle() {
    start_timer(kWaitTickUs, kPauseRepeats);
    m_state = State::Idle;
}

void DS18X20::next_slot_or_idle() {
    m_attempts = 0;
    if (static_cast<uint8_t>(m_slot + 1) < DS18X20_SENSOR_COUNT) {
        ++m_slot;
        reset_bus();
        m_state = State::SlotSelect;
    } else {
        pause_or_idle();
    }
}

void DS18X20::init() {
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_DMA1EN;
    TIM1->PSC = kTimPrescaler;
    TIM1->BDTR = TIM_BDTR_MOE;
    configure_measurement_pin();
    rearm();
}

void DS18X20::rearm() {
    configure_measurement_pin();
    TIM1->CR1 = 0;
    TIM1->CCER = 0;
    TIM1->DIER = 0;
    m_state = State::Idle;
    m_slot = 0;
    m_attempts = 0;
    for (uint8_t i = 0; i < DS18X20_SENSOR_COUNT; ++i) {
        m_last_temp[i] = TEMP_ERROR_NO_SENSOR;
    }
    TIM1->EGR = TIM_EGR_UG; // UIF, чтобы первый poll() стартовал цикл
}

void DS18X20::poll() {
    if (!(TIM1->SR & TIM_SR_UIF)) return;
    ds18x20_hw::clear_uif();

    switch (m_state) {
        case State::Idle:
            m_slot = 0;
            m_attempts = 0;
            reset_bus();
            m_state = State::Convert;
            break;

        case State::Convert:
            if (!check_presence()) {
                emit_bus_absent();
                break;
            }
            send_command(kConvertCmd.data(), kSkipCmdBits);
            m_state = State::Wait;
            break;

        case State::Wait:
            start_timer(kWaitTickUs, kConvertRepeats);
            m_state = State::SlotReset;
            break;

        case State::SlotReset:
            reset_bus();
            m_state = State::SlotSelect;
            break;

        case State::SlotSelect:
            if (!check_presence()) {
                emit_bus_absent();
                break;
            }
            send_slot_read();
            m_state = State::SlotRead;
            break;

        case State::SlotRead:
            read_data();
            m_state = State::SlotDecode;
            break;

        case State::SlotDecode:
            decode_and_report();
            break;
    }
}
