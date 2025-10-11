#include "uart.hpp"

// ======== Config: printing buffer size (power of two) ========
#ifndef UART_TX_BUF_SIZE
#define UART_TX_BUF_SIZE 128u     // set to 128 or 256 as desired
#endif

// Validate that buffer size is a power of two for efficient masking operations
#if (UART_TX_BUF_SIZE & (UART_TX_BUF_SIZE - 1u)) != 0
#error "UART_TX_BUF_SIZE must be a power of two (e.g., 32, 64, 128, 256)."
#endif

// Create mask for buffer index wrapping (power of two optimization)
#define UART_TX_IDX_MASK (UART_TX_BUF_SIZE - 1u)
// Calculate baud rate register value with rounding for accuracy
#define USART_BRR_CALC(PCLK, BAUD) (((PCLK) + ((BAUD)/2)) / (BAUD))

// ======== USART1 TX ring buffer ========
static uint8_t uart_tx_head = 0;                     // write index - points to next free slot
static uint8_t uart_tx_tail = 0;                     // read index - points to oldest data
static uint8_t uart_tx_buf[UART_TX_BUF_SIZE];        // circular buffer for UART transmission

/**
 * @brief Non-blocking function to enqueue a single byte into the UART transmit buffer
 * @param[in] b Byte to enqueue
 * @return 1 on success, 0 if buffer full
 * @note Returns immediately without blocking
 */
__STATIC_FORCEINLINE int uart_tx_enqueue_byte(uint8_t b) {
    uint8_t head = uart_tx_head;
    // Calculate next head position with wrap-around using power-of-two mask
    auto next = (uint8_t) ((head + 1u) & UART_TX_IDX_MASK);
    if (next == uart_tx_tail) { // Check if buffer is full
        return 0; // Buffer full - non-blocking return
    }

    uart_tx_buf[head] = b; // Store byte at current head position
    uart_tx_head = next;   // Atomically update head pointer
    return 1; // Success
}

/**
 * @brief Non-blocking function to enqueue entire null-terminated string into UART transmit buffer
 * @param[in] s Null-terminated string to enqueue
 * @return Number of characters successfully enqueued
 */
int uart_write_str(const char *s) {
    const char *start = s;
    // Process each character until null terminator
    while (*s) {
        // Try to enqueue current character, break on buffer full (non-blocking)
        if (!uart_tx_enqueue_byte((uint8_t) *s)) break;
        s++;
    }
    // Return count of successfully enqueued characters
    return (int) (s - start);
}

/**
 * @brief Non-blocking function to advance UART transmission by at most one byte
 * @note Must be called periodically to feed UART hardware from buffer
 * @note Returns immediately without blocking
 */
void uart_poll_tx() {
    // Check if UART is ready to transmit (TXE flag set) and buffer not empty
    if ((USART1->ISR & USART_ISR_TXE) && (uart_tx_tail != uart_tx_head)) {
        // Get byte from buffer at tail position
        uint8_t b = uart_tx_buf[uart_tx_tail];
        // Advance tail pointer with wrap-around
        uart_tx_tail = (uint8_t) ((uart_tx_tail + 1u) & UART_TX_IDX_MASK);
        // Write byte to UART data register for transmission
        USART1->TDR = b;
    }
}

/**
 * @brief Initialize microcontroller peripherals for UART communication and LED control
 */
void hardware_init() {
    /* --- 1. Включаем тактирование --- */
    RCC->AHBENR |= RCC_AHBENR_GPIOAEN;    // GPIOA
    RCC->APB2ENR |= RCC_APB2ENR_USART1EN; // USART1

    /* --- 2. Настраиваем пины PA9 (TX) и PA10 (RX) --- */
    GPIOA->MODER &= ~((3U << (9 * 2)) | (3U << (10 * 2))); // Сбрасываем режим
    GPIOA->MODER |= ((2U << (9 * 2)) | (2U << (10 * 2))); // Alternate function

    GPIOA->AFR[1] &= ~((0xF << ((9 - 8) * 4)) | (0xF << ((10 - 8) * 4)));
    GPIOA->AFR[1] |= ((1U << ((9 - 8) * 4)) | (1U << ((10 - 8) * 4))); // AF1 = USART1

    GPIOA->OSPEEDR |= ((3U << (9 * 2)) | (3U << (10 * 2))); // High speed
    GPIOA->OTYPER &= ~((1U << 9) | (1U << 10));             // Push-pull
    GPIOA->PUPDR &= ~((3U << (9 * 2)) | (3U << (10 * 2))); // No pull

    /* --- 3. Настраиваем USART1 --- */
    USART1->CR1 = 0; // Сбрасываем на всякий случай

    // Baud rate: 48 MHz / 115200 = 416.666 → BRR ≈ 417
    USART1->BRR = 417U;

    // 8 бит, без чётности, 1 стоп, без flow control
    USART1->CR1 |= USART_CR1_TE | USART_CR1_RE; // Включаем TX и RX

    /* --- 4. Включаем USART --- */
    USART1->CR1 |= USART_CR1_UE;

    /* --- 5. Ждём готовности --- */
    while (!(USART1->ISR & USART_ISR_TEACK));
    while (!(USART1->ISR & USART_ISR_REACK));
}

/**
 * @brief Weak implementation for DS18B20 LED control - provides visual feedback
 * @param[in] action 0 to turn LED off, non-zero to turn LED on
 * @note Non-blocking LED control using atomic BSRR register operations
 */
void ds18b20_led_control(unsigned action) {
    if (action) {
        // Turn LED on (PC13 low due to pull-up LED configuration)
        // BSRR BR register: atomic bit reset operation
        GPIOB->BSRR = GPIO_BSRR_BR_0;
    } else {
        // Turn LED off (PC13 high)
        // BSRR BS register: atomic bit set operation
        GPIOB->BSRR = GPIO_BSRR_BS_0;
    }
}

/**
 * @brief Convert integer to string and enqueue for UART transmission
 * @param[in] value Integer value to convert and transmit
 */
void uart_write_int(int value) {
    char buf[7];       // enough for -32768 and '\0'
    char *p = buf + sizeof(buf) - 1;
    *p = '\0';

    if (value == 0) {  // Special case for zero
        *(--p) = '0';
    } else {
        int is_negative = 0;
        unsigned int uvalue = value;

        if (value < 0) {  // Handle negative numbers
            is_negative = 1;
            uvalue = -value;
        }

        do {  // Convert digits from least significant to most significant
            *(--p) = '0' + (uvalue % 10);
            uvalue /= 10;
        } while (uvalue);

        if (is_negative) *(--p) = '-'; // Add negative sign if needed
    }
    (void) uart_write_str(p); // best-effort enqueue to UART buffer
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
    if (temp == DS18B20_TEMP_ERROR_NO_SENSOR) { // No sensor detected error - enqueue error message
        uart_write_str("DS18B20 error: no sensor detected.\r\n");
    } else if (temp == DS18B20_TEMP_ERROR_CRC_FAIL) { // CRC check failed error - enqueue error message
        uart_write_str("DS18B20 error: CRC check failed.\r\n");
    } else if (temp == DS18B20_TEMP_ERROR_GENERIC) { // Generic error - enqueue error message
        uart_write_str("DS18B20 error: generic failure.\r\n");
    } else {                                 // Valid temperature reading - format and display
        int whole = temp / 10;               // Get whole degrees (temp is in tenths)
        int frac = temp % 10;                // Get fractional part (tenths)
        if (frac < 0) frac = -frac;          // Ensure fractional part is positive
        uart_write_str("Temperature: ");
        uart_write_int(whole);          // Display whole part
        uart_write_str(".");               // Decimal point
        uart_write_int(frac);           // Display fractional part
        uart_write_str(" C");              // Units
#if defined ELAPSED_TIME
        uart_write_str(" (");   // Parenthesis
        uart_write_int(t / 72); // Display time elapsed
        uart_write_str(" us)"); // Parenthesis
#endif
        uart_write_str("\r\n");     // And newline
    }
}
