#include "ds18b20.hpp"

#include <cstdint>
#include <array>

#include "EventQueue.hpp"
#include "AppContext.hpp"

// #define PRINT_TEMP

extern App app;

#if defined ELAPSED_TIME
static uint32_t elapsed_time;
#endif

/**
 * @defgroup DS18B20_Private_Constants DS18B20 Private Constants
 * @{
 */

/** @brief Timer configuration for 1µs resolution (72MHz system clock / 72 = 1MHz) */
#define TIM_PRESCALER         47
/** @brief Minimum reset pulse duration in microseconds */
#define RESET_PULSE_MIN       480U
/** @brief Maximum reset pulse duration in microseconds */
#define RESET_PULSE_MAX       540U
/** @brief Minimum presence pulse positive width in microseconds */
#define POSITIVE_WIDTH_MIN    15U
/** @brief Maximum presence pulse positive width in microseconds */
#define POSITIVE_WIDTH_MAX    60U
/** @brief Minimum presence pulse negative width in microseconds */
#define NEGATIVE_WIDTH_MIN    60U
/** @brief Maximum presence pulse negative width in microseconds */
#define NEGATIVE_WIDTH_MAX    240U
/** @brief Calculated minimum presence pulse timing */
#define PRESENCE_PULSE_MIN    (RESET_PULSE_MIN + POSITIVE_WIDTH_MIN + NEGATIVE_WIDTH_MIN)
/** @brief Calculated maximum presence pulse timing */
#define PRESENCE_PULSE_MAX    (RESET_PULSE_MAX + POSITIVE_WIDTH_MAX + NEGATIVE_WIDTH_MAX)
/** @brief Duration to drive bus low during reset in microseconds */
#define RESET_PULSE_DURATION  RESET_PULSE_MIN
/** @brief Total reset timeslot timeout in microseconds */
#define RESET_TIMEOUT         (RESET_PULSE_MIN * 2)
/** @brief CRC8 polynomial for DS18B20 scratchpad validation (Dallas/Maxim algorithm) */
#define DS18B20_CRC8_POLY     0x8C
/** @brief Number of bytes to include in CRC calculation */
#define DS18B20_CRC8_BYTES    8
/** @brief Size of edge capture buffer for presence detection */
#define CAPTURE_BUF_SIZE      2
/** @brief Total length of DS18B20 scratchpad in bytes */
#define DS18B20_SCRATCHPAD_LEN   9
/** @brief Standard 8 bits per byte */
#define DS18B20_BITS_PER_BYTE    8
/** @brief Total number of bits in DS18B20 scratchpad */
#define DS18B20_SCRATCHPAD_BITS (DS18B20_SCRATCHPAD_LEN * DS18B20_BITS_PER_BYTE)
/** @brief Threshold to distinguish short/long pulses (10µs) */
#define SHORT_PULSE_MAX       0x0A
/** @brief Number of DMA transfers for command transmission */
#define DS18B20_DMA_TRANSFERS   16

// Константы для длительностей импульсов
constexpr uint8_t ONE_PULSE = 1;   // 1 µs
constexpr uint8_t ZERO_PULSE = 60;  // 60 µs

// Преобразование одного бита
constexpr uint8_t bitToPulse(uint8_t byte, uint8_t bit) noexcept {
    return (byte & (1u << bit)) ? ONE_PULSE : ZERO_PULSE;
}

// Преобразование байта в 8 длительностей
constexpr std::array<uint8_t, 8> byteToPulses(uint8_t byte) noexcept {
    std::array<uint8_t, 8> pulses{};
    for (uint8_t i = 0; i < 8; ++i)
        pulses[i] = bitToPulse(byte, i);
    return pulses;
}

// Преобразование массива байтов в объединённую команду
template<std::size_t N>
constexpr auto makeCommand(const std::array<uint8_t, N> &bytes) noexcept {
    std::array<uint8_t, N * 8 + 1> cmd{}; // +1 для завершающего 0
    for (std::size_t i = 0; i < N; ++i)
        for (std::size_t bit = 0; bit < 8; ++bit)
            cmd[i * 8 + bit] = bitToPulse(bytes[i], bit);
    cmd[N * 8] = 0;
    return cmd;
}

// Использование
constexpr auto conv_cmd = makeCommand(std::array<uint8_t, 2>{0xCC, 0x44});
constexpr auto read_cmd = makeCommand(std::array<uint8_t, 2>{0xCC, 0xBE});

void DS18B20::detect_sensor_type() {
    // DS18S20 не имеет конфигурационного регистра - scratchpad[4] = 0xFF
    if (m_ctx.scratchpad[4] == 0xFF) {
        m_family = 0x10;   // DS18S20
    } else {
        m_family = 0x28;   // DS18B20
    }
}

void DS18B20::ForceUpdateEvent(TIM_TypeDef *tim) {
    tim->EGR = TIM_EGR_UG;                 // Сгенерировать событие обновления
    //while (!(tim->SR & TIM_SR_UIF)) {}     // Дождаться флага обновления
    tim->SR &= ~TIM_SR_UIF;                // Сбросить флаг
}

/**
 * @}
 */

/**
 * @defgroup DS18B20_Private_Functions DS18B20 Private Functions
 * @{
 */

/**
 * @brief Weak implementation for DS18B20 LED control - provides visual feedback
 * @param[in] action 0 to turn LED off, non-zero to turn LED on
 * @note Non-blocking LED control using atomic BSRR register operations
 */
void ds18b20_led_control(unsigned action) {
    if (action) {
        // Turn LED on (PC13 low due to pull-up LED configuration)
        // BSRR BR register: atomic bit reset operation
        GPIOA->BSRR = GPIO_BSRR_BR_11;
    } else {
        // Turn LED off (PC13 high)
        // BSRR BS register: atomic bit set operation
        GPIOA->BSRR = GPIO_BSRR_BS_11;
    }
}

/**
 * @brief Weak implementation for DS18B20 temperature ready callback - handles temperature display
 * @param[in] temp Temperature value in tenths of degrees Celsius, or error code
 */
#if defined ELAPSED_TIME
void ds18b20_temp_ready(int16_t temp, uint32_t t) {
#else

void ds18b20_temp_ready(int16_t temp) {
#endif
    if (temp == DS18B20::ErrorStatus::TEMP_ERROR_NO_SENSOR) { // No sensor detected error - enqueue error message
        app.uart->write_str("DS18B20 error: no sensor detected.\r\n");
    } else if (temp == DS18B20::ErrorStatus::TEMP_ERROR_CRC_FAIL) { // CRC check failed error - enqueue error message
        app.uart->write_str("DS18B20 error: CRC check failed.\r\n");
    } else if (temp == DS18B20::ErrorStatus::TEMP_ERROR_GENERIC) { // Generic error - enqueue error message
        app.uart->write_str("DS18B20 error: generic failure.\r\n");
    } else {                                 // Valid temperature reading - format and display
        int whole = temp / 10;               // Get whole degrees (temp is in tenths)
        int frac = temp % 10;                // Get fractional part (tenths)
        if (frac < 0) frac = -frac;          // Ensure fractional part is positive
#if defined PRINT_TEMP
        app.uart->write_str("Temperature: ");
        app.uart->write_int(whole);          // Display whole part
        app.uart->write_str(".");               // Decimal point
        app.uart->write_int(frac);           // Display fractional part
        app.uart->write_str(" C");              // Units
#if defined ELAPSED_TIME
        app.uart->write_str(" (");   // Parenthesis
        app.uart->write_int(t / 48); // Display time elapsed
        app.uart->write_str(" us)"); // Parenthesis
#endif
        app.uart->write_str("\r\n");     // And newline
#endif

        if (app.queue) {
            // temp уже в десятых долях градуса, передаём как есть
            app.queue->push({EventType::TemperatureReady, temp});
        }
    }
}

/**
 * @brief Calculate CRC8 checksum for DS18B20 scratchpad data validation
 * @return CRC8 checksum value
 */
uint8_t DS18B20::check_scratchpad_crc() {
    uint8_t crc = 0;
    // Process each byte in the scratchpad (first 8 bytes) for CRC calculation
    for (uint8_t i = 0; i < DS18B20_CRC8_BYTES; i++) {
        uint8_t inByte = m_ctx.scratchpad[i];
        // Process each bit in the byte using Dallas/Maxim CRC8 algorithm
        for (uint8_t b = 0; b < 8; b++) {
            uint8_t mix = (crc ^ inByte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= DS18B20_CRC8_POLY;
            inByte >>= 1;
        }
    }
    return crc;
}

/**
 * @brief Decode pulse durations into scratchpad bytes using bit timing analysis
 */
void DS18B20::decode_scratchpad() {
    // Process each byte in the scratchpad (9 bytes total)
    for (unsigned byte = 0; byte < DS18B20_SCRATCHPAD_LEN; ++byte) {
        const unsigned bit_start = byte * DS18B20_BITS_PER_BYTE;
        // Process each bit in the byte (8 bits per byte)
        for (unsigned bit = 0; bit < DS18B20_BITS_PER_BYTE; ++bit) {
            // Determine if pulse represents logic '1' or '0' based on duration threshold
            // Pulses <= 10µs are considered logic '1', > 10µs are logic '0'
            if (m_ctx.pulse[bit_start + bit] <= SHORT_PULSE_MAX) {
                m_ctx.scratchpad[byte] |= (1 << bit);  // Set bit to 1
            } else {
                m_ctx.scratchpad[byte] &= ~(1 << bit);  // Reset bit to 0
            }
        }
    }
}

/**
 * @brief Convert raw temperature data from scratchpad to tenths of degrees Celsius
 * @return Temperature value in tenths of degrees Celsius
 */
int16_t DS18B20::decode_temperature() {
    // Combine LSB and MSB of temperature register (bytes 0 and 1)
    auto raw = (int16_t) ((m_ctx.scratchpad[1] << 8) | m_ctx.scratchpad[0]);

    if (m_family == 0x10) {
        int16_t cnt_rem = m_ctx.scratchpad[6];
        int16_t cnt_per_c = m_ctx.scratchpad[7];

        int32_t coarse = (raw >> 1) * 10;
        int32_t fine = ((cnt_per_c - cnt_rem) * 10) / cnt_per_c;

        return coarse - 2 + fine;
    }
    return static_cast<int16_t>((raw * 10) >> 4);
}

/**
 * @brief Verify presence of DS18B20 sensor by checking reset pulse timing
 * @return 1 if device present, 0 if no device detected
 */
bool DS18B20::check_presence() const {
    // Validate that reset pulse duration is within specification
    // and presence pulse timing indicates a responding device
    return (m_ctx.edge[0] >= RESET_PULSE_MIN) && (m_ctx.edge[0] <= RESET_PULSE_MAX) &&
           (m_ctx.edge[1] >= PRESENCE_PULSE_MIN) && (m_ctx.edge[1] <= PRESENCE_PULSE_MAX);
}

/**
 * @brief Start timer with specified period and repetition count for precise timing
 * @param[in] arr Auto-reload register value
 * @param[in] rcr Repetition counter value
 */
void DS18B20::start_timer(uint16_t arr, uint8_t rcr) {
    TIM1->ARR = arr;
    TIM1->RCR = rcr;
    // Force update event to load new values
    ForceUpdateEvent(TIM1);
    // Start timer in One Pulse Mode (OPM) - runs once then stops
    TIM1->CR1 = TIM_CR1_OPM | TIM_CR1_CEN;
}

/**
 * @brief Initialize 1-Wire bus reset sequence using timer and DMA
 */
void DS18B20::reset_bus() {
    // Configure timer for reset pulse generation (480µs low)
    TIM1->ARR = RESET_TIMEOUT;              // Total reset slot time (960µs)
    TIM1->CCR1 = RESET_PULSE_DURATION;      // Reset pulse duration (480µs)
    // Configure channel 1 for output compare (drive bus low)
    // Configure channel 2 for input capture (detect presence pulse)
    TIM1->CCMR1 = (TIM_CCMR1_OC1M_0 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1PE) |
                  (TIM_CCMR1_CC2S_1 | TIM_CCMR1_IC2F_0 | TIM_CCMR1_IC2F_1 | TIM_CCMR1_IC2F_2);
    TIM1->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E;      // Enable both channels
    TIM1->RCR = 0;                                  // No repetition
    // Configure DMA to capture presence pulse edge timestamps
    DMA1_Channel3->CCR = 0;                           // Clear DMA configuration
    DMA1_Channel3->CPAR = (uint32_t) &TIM1->CCR2;      // DMA destination: timer capture register
    DMA1_Channel3->CMAR = (uint32_t) m_ctx.edge;        // DMA source: edge timestamp buffer
    DMA1_Channel3->CNDTR = CAPTURE_BUF_SIZE;          // Number of transfers (2 edges)
    // Enable DMA with memory increment
    DMA1_Channel3->CCR = DMA_CCR_MINC | DMA_CCR_PSIZE_0 | DMA_CCR_MSIZE_0 | DMA_CCR_EN;
    // Force timer update to load configuration
    ForceUpdateEvent(TIM1);
    TIM1->CCR1 = 0;                           // Clear output compare value
    TIM1->DIER = TIM_DIER_CC2DE;              // Enable DMA request on capture
    TIM1->CR1 = TIM_CR1_OPM | TIM_CR1_CEN;   // Start timer in one-pulse mode
}

/**
 * @brief Transmit command sequence to DS18B20 using DMA
 * @param[in] cmd Pointer to command sequence in pulse duration format
 * @note Non-blocking - configures hardware to transmit command automatically
 */
void DS18B20::send_command(const uint8_t *cmd) {
    // Configure timer for command transmission using DMA
    TIM1->RCR = DS18B20_DMA_TRANSFERS - 1;   // Number of repetitions (16 transfers)
    TIM1->ARR = ONE_PULSE + ZERO_PULSE + 1;  // Total bit slot time (62µs)
    TIM1->CCR1 = cmd[0];                     // First pulse duration
    TIM1->CCR4 = ONE_PULSE + ZERO_PULSE;     // Update trigger time
    // Configure channel 1 for output compare mode
    TIM1->CCMR1 = TIM_CCMR1_OC1M_0 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2;
    TIM1->CCER = TIM_CCER_CC1E;             // Enable output compare
    TIM1->DIER = TIM_DIER_CC4DE;            // Enable DMA request on update
    // Force timer update to load configuration
    ForceUpdateEvent(TIM1);
    // Configure DMA to transmit command pulse sequence
    DMA1_Channel4->CCR = 0;                          // Clear DMA configuration
    DMA1_Channel4->CPAR = (uint32_t) &TIM1->CCR1;    // DMA destination: output compare register
    DMA1_Channel4->CMAR = (uint32_t) &cmd[1];        // DMA source: command data (skip first byte)
    DMA1_Channel4->CNDTR = DS18B20_DMA_TRANSFERS;    // Number of transfers
    // Enable DMA with memory increment
    DMA1_Channel4->CCR = DMA_CCR_DIR | DMA_CCR_MINC | DMA_CCR_PSIZE_0 | DMA_CCR_EN;
    TIM1->CR1 = TIM_CR1_OPM | TIM_CR1_CEN;           // Start timer in one-pulse mode
}

/**
 * @brief Read scratchpad data from DS18B20 using timer capture and DMA
 * @note Non-blocking - configures hardware to capture data automatically
 */
void DS18B20::read_data() {
    // Configure timer for data reading with input capture
    TIM1->RCR = DS18B20_SCRATCHPAD_BITS - 1; // Number of repetitions (72 bits)
    TIM1->ARR = ONE_PULSE + ZERO_PULSE + 1;  // Total bit slot time (62µs)
    TIM1->CCR1 = ONE_PULSE;                  // Read pulse duration (1µs)
    // Configure channel 1 for output compare (generate read pulse)
    // Configure channel 2 for input capture (measure return pulse durations)
    TIM1->CCMR1 = (TIM_CCMR1_OC1M_0 | TIM_CCMR1_OC1M_1 | TIM_CCMR1_OC1M_2 | TIM_CCMR1_OC1PE) |
                  (TIM_CCMR1_CC2S_1 | TIM_CCMR1_IC2F_0 | TIM_CCMR1_IC2F_1 | TIM_CCMR1_IC2F_2);
    TIM1->CCER = TIM_CCER_CC1E | TIM_CCER_CC2E;   // Enable both channels
    TIM1->DIER = TIM_DIER_CC2DE;                  // Enable DMA request on capture
    // Force timer update to load configuration
    ForceUpdateEvent(TIM1);
    TIM1->CCR1 = 0;                          // Clear output compare value
    // Configure DMA to capture pulse durations into pulse buffer
    DMA1_Channel3->CCR = 0;                                            // Clear DMA configuration
    DMA1_Channel3->CPAR = (uint32_t) &TIM1->CCR2;                       // DMA destination: capture register
    DMA1_Channel3->CMAR = (uint32_t) m_ctx.pulse;                        // DMA source: pulse duration buffer
    DMA1_Channel3->CNDTR = DS18B20_SCRATCHPAD_BITS;                    // Number of transfers (72 bits)
    DMA1_Channel3->CCR = DMA_CCR_MINC | DMA_CCR_PSIZE_0 | DMA_CCR_EN;  // Enable DMA with memory increment
    TIM1->CR1 = TIM_CR1_OPM | TIM_CR1_CEN;   // Start timer in one-pulse mode
}

/**
 * @}
 */

/**
 * @defgroup DS18B20_Public_Functions DS18B20 Public Functions
 * @{
 */

// Методы-действия для состояний FSM
void DS18B20::action_idle() {
#if defined ELAPSED_TIME
    elapsed_time = DWT->CYCCNT;
#endif
    // Initialize union memory (fills with 0xFF pattern)
    m_ctx.fill_union = (uint64_t) -1;
}

void DS18B20::action_start() {
    // Turn on LED to indicate measurement in progress
    ds18b20_led_control(!0);
    // Initiate 1-Wire bus reset sequence
    reset_bus();
}

void DS18B20::action_convert_ok() {
    // Device present - send temperature conversion command
    send_command(conv_cmd.data());
}

void DS18B20::action_convert_fail() {
    // No device present - report error and pause
#if defined ELAPSED_TIME
    ds18b20_temp_ready(ErrorStatus::TEMP_ERROR_NO_SENSOR, DWT->CYCCNT - elapsed_time);
#else
    ds18b20_temp_ready(ErrorStatus::TEMP_ERROR_NO_SENSOR);
#endif
    // Start inter-measurement pause
    start_cycle_pause();
}

void DS18B20::action_wait() {
    // Start timer for conversion wait period (750ms typical)
    wait_conversion();
}

void DS18B20::action_continue() {
    // Initiate second 1-Wire bus reset sequence
    reset_bus();
}

void DS18B20::action_request_ok() {
    // Device present - send read scratchpad command
    send_command(read_cmd.data());
}

void DS18B20::action_request_fail() {
    // No device present - report error and pause
#if defined ELAPSED_TIME
    ds18b20_temp_ready(ErrorStatus::TEMP_ERROR_NO_SENSOR, DWT->CYCCNT - elapsed_time);
#else
    ds18b20_temp_ready(ErrorStatus::TEMP_ERROR_NO_SENSOR);
#endif
    // Start inter-measurement pause
    start_cycle_pause();
}

void DS18B20::action_read() {
    // Initiate scratchpad data read using timer capture and DMA
    read_data();
}

void DS18B20::action_decode() {
    // Decode captured pulse durations into scratchpad bytes
    decode_scratchpad();
    detect_sensor_type();
    // Turn off LED to indicate measurement complete
    ds18b20_led_control(0);

    // Validate CRC and report temperature or error
#if defined ELAPSED_TIME
    if (m_ctx.scratchpad[8] == check_scratchpad_crc()) {
        // CRC valid - decode and report temperature
        ds18b20_temp_ready(decode_temperature(), DWT->CYCCNT - elapsed_time);
    } else {
        // CRC invalid - report error
        ds18b20_temp_ready(ErrorStatus::TEMP_ERROR_CRC_FAIL, DWT->CYCCNT - elapsed_time);
    }
#else
    if (m_ctx.scratchpad[8] == check_scratchpad_crc()) {
        // CRC valid - decode and report temperature
        ds18b20_temp_ready(decode_temperature());
    } else {
        // CRC invalid - report error
        ds18b20_temp_ready(ErrorStatus::TEMP_ERROR_CRC_FAIL);
    }
#endif

    // Start inter-measurement pause period
    start_cycle_pause();
}

// Таблица переходов FSM
const DS18B20::Transition DS18B20::m_transitions[] = {
        // IDLE -> START (безусловный, fallthrough - выполняем action_idle и сразу переходим в START)
        {FsmStates::IDLE,     nullptr,                       &DS18B20::action_idle,         FsmStates::START},

        // START -> CONVERT (безусловный)
        {FsmStates::START,    nullptr,                       &DS18B20::action_start,        FsmStates::CONVERT},

        // CONVERT -> WAIT (если присутствует)
        {FsmStates::CONVERT,  &DS18B20::check_presence_ok,   &DS18B20::action_convert_ok,   FsmStates::WAIT},

        // CONVERT -> IDLE (если отсутствует)
        {FsmStates::CONVERT,  &DS18B20::check_presence_fail, &DS18B20::action_convert_fail, FsmStates::IDLE},

        // WAIT -> CONTINUE (безусловный)
        {FsmStates::WAIT,     nullptr,                       &DS18B20::action_wait,         FsmStates::CONTINUE},

        // CONTINUE -> REQUEST (безусловный)
        {FsmStates::CONTINUE, nullptr,                       &DS18B20::action_continue,     FsmStates::REQUEST},

        // REQUEST -> READ (если присутствует)
        {FsmStates::REQUEST,  &DS18B20::check_presence_ok,   &DS18B20::action_request_ok,   FsmStates::READ},

        // REQUEST -> IDLE (если отсутствует)
        {FsmStates::REQUEST,  &DS18B20::check_presence_fail, &DS18B20::action_request_fail, FsmStates::IDLE},

        // READ -> DECODE (безусловный)
        {FsmStates::READ,     nullptr,                       &DS18B20::action_read,         FsmStates::DECODE},

        // DECODE -> IDLE (безусловный, CRC проверяется внутри)
        {FsmStates::DECODE,   nullptr,                       &DS18B20::action_decode,       FsmStates::IDLE},
};

/**
 * @brief Initialize DS18B20 driver - configure clocks and peripherals
 */
void DS18B20::init() {
    // Enable clocks for required peripherals: GPIOA, TIM1, DMA1
    RCC->APB2ENR |= RCC_APB2ENR_TIM1EN;
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN | RCC_AHBENR_DMA1EN;
    // Configure timer prescaler for 1µs resolution (72MHz/72 = 1MHz)
    TIM1->PSC = TIM_PRESCALER;
    TIM1->EGR = TIM_EGR_UG;
    TIM1->BDTR = TIM_BDTR_MOE;

    // Configure PA8 for 1-Wire communication (alternate function open drain)
    // 1. Включаем тактирование GPIOA
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;

    // 2. Настраиваем PA8 как "Alternate Function"
    GPIOA->MODER &= ~(3U << (8 * 2));   // очистить 2 бита (16 и 17)
    GPIOA->MODER |= (2U << (8 * 2));   // установить 10b = Alternate Function

    // 3. Настраиваем тип выхода (Push-Pull)
    GPIOA->OTYPER &= ~(1U << 8);        // 0 = Push-Pull

    // 4. Настраиваем скорость
    GPIOA->OSPEEDR |= (3U << (8 * 2));  // 11b = High speed

    // 5. Настраиваем без подтяжек
    GPIOA->PUPDR &= ~(3U << (8 * 2));   // 00b = No pull-up/pull-down

    // 6. Выбираем альтернативную функцию AF2 для TIM1_CH1
    // У PA8 альтернативные функции задаются в AFRH (пины 8..15)
    GPIOA->AFR[1] &= ~(0xFU << ((8 - 8) * 4)); // очистить 4 бита
    GPIOA->AFR[1] |= (0x2U << ((8 - 8) * 4)); // установить AF2
}

/**
 * @brief Main state machine function - must be called periodically from main loop
 * @note Non-blocking state machine that advances 1-Wire communication state
 * @note Uses timer update interrupt flag to determine when operations complete
 */
void DS18B20::poll() {
    // Check if timer update interrupt occurred (indicates operation completion)
    // This is the non-blocking way to detect when timed operations finish
    if (!(TIM1->SR & TIM_SR_UIF)) return;
    // Clear timer update interrupt flag
    TIM1->SR = 0;

    // Поиск подходящего перехода в таблице
    bool transition_found = false;
    bool need_fallthrough = false;

    for (const auto &t: m_transitions) {
        if (t.state == m_ctx.current_state) {
            // Проверка условия (если есть)
            if (!t.guard || (this->*t.guard)()) {
                // Выполнение действия
                (this->*t.action)();
                // Переход в следующее состояние
                m_ctx.current_state = t.next;
                transition_found = true;

                // Обработка fallthrough: IDLE -> START выполняется сразу
                if (t.next == FsmStates::START) {
                    need_fallthrough = true;
                    // Продолжаем поиск для START
                    continue;
                }
                break;
            }
        }
    }

    // Если нужен fallthrough (IDLE -> START), выполняем START -> CONVERT сразу
    if (need_fallthrough && m_ctx.current_state == FsmStates::START) {
        for (const auto &t: m_transitions) {
            if (t.state == FsmStates::START) {
                if (!t.guard || (this->*t.guard)()) {
                    (this->*t.action)();
                    m_ctx.current_state = t.next;
                    break;
                }
            }
        }
    }

    // Если переход не найден - ошибка
    if (!transition_found) {
        // Unexpected state - report generic error
#if defined ELAPSED_TIME
        ds18b20_temp_ready(ErrorStatus::TEMP_ERROR_GENERIC, DWT->CYCCNT - elapsed_time);
#else
        ds18b20_temp_ready(ErrorStatus::TEMP_ERROR_GENERIC);
#endif
        // Return to IDLE state
        m_ctx.current_state = FsmStates::IDLE;
    }
}

/**
 * @}
 */
