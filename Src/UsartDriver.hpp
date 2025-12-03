#pragma once

#include "stm32f0xx.h"
#include "GpioDriver.hpp"

template<uint32_t Baudrate = 115200,
        uint16_t TxBuffSize = 256>
class UsartDriver {
    /**
     * @brief USART1 TX ring buffer
     */
    uint8_t tx_head = 0;               ///< write index - points to next free slot
    uint8_t tx_tail = 0;               ///< read index - points to the oldest data
    uint8_t tx_buf[TxBuffSize] = {};   ///< circular buffer for UART transmission

    static inline void EnableClock() {
        if (!(RCC->APB2ENR & RCC_APB2ENR_USART1EN)) {
            RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
            __DSB();
        }
    }

    static inline void DisableClock() {
        if (RCC->APB2ENR & RCC_APB2ENR_USART1EN) {
            RCC->APB2ENR &= ~RCC_APB2ENR_USART1EN;
            __DSB();
        }
    }

    /**
     * @brief Non-blocking function to enqueue a single byte into the UART transmit buffer
     * @param[in] b Byte to enqueue
     * @return 1 on success, 0 if buffer full
     * @note Returns immediately without blocking
     */
    inline int tx_enqueue_byte(uint8_t b) {
        uint8_t head = tx_head;
        // Calculate next head position with wrap-around using power-of-two mask
        uint8_t next = (uint8_t) ((head + 1U) & (TxBuffSize - 1U));
        if (next == tx_tail) { // Check if buffer is full
            return 0; // Buffer full - non-blocking return
        }

        tx_buf[head] = b; // Store byte at current head position
        tx_head = next;   // Atomically update head pointer
        return 1; // Success
    }

public:
    static inline void Init(uint32_t apb_clk_hz) {
        GpioDriver rx(GPIOA, 10);
        GpioDriver tx(GPIOA, 9);

        rx.Init(GpioDriver::Mode::Alternate);
        rx.SetAlternateFunction(1);

        tx.Init(GpioDriver::Mode::Alternate);
        tx.SetAlternateFunction(1);

        EnableClock();

        RCC->CFGR3 &= ~RCC_CFGR3_USART1SW;
        RCC->CFGR3 |= RCC_CFGR3_USART1SW_0; // SYSCLK

        USART1->BRR = (apb_clk_hz + Baudrate / 2) / Baudrate;
        USART1->CR1 = USART_CR1_RE | USART_CR1_TE;
        USART1->CR2 = 0;
        USART1->CR1 |= USART_CR1_UE; // Enable USART

        //USART()->CR1 |= USART_CR1_RXNEIE;
        //NVIC_EnableIRQ(USART1_IRQn);
    }

    /**
     * @brief Non-blocking function to enqueue entire null-terminated string into UART transmit buffer
     * @param[in] s Null-terminated string to enqueue
     * @return Number of characters successfully enqueued
     */
    inline int write_str(const char *s) {
        const char *start = s;
        // Process each character until null terminator
        while (*s) {
            // Try to enqueue current character, break on buffer full (non-blocking)
            if (!tx_enqueue_byte((uint8_t) *s)) break;
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
    inline void poll_tx() {
        // Check if UART is ready to transmit (TXE flag set) and buffer not empty
        if ((USART1->ISR & USART_ISR_TXE) && (tx_tail != tx_head)) {
            // Get byte from buffer at tail position
            uint8_t b = tx_buf[tx_tail];
            // Advance tail pointer with wrap-around
            tx_tail = (uint8_t) ((tx_tail + 1U) & (TxBuffSize - 1U));
            // Write byte to UART data register for transmission
            USART1->TDR = b;
        }
    }

    /**
     * @brief Convert integer to string and enqueue for UART transmission
     * @param[in] value Integer value to convert and transmit
     */
    inline void write_int(int value) {
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
        (void) write_str(p); // Best-effort enqueue to UART buffer
    }
};