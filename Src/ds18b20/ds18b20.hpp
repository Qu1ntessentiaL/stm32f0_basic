/**
 * @file ds18b20.h
 * @brief Non-blocking DS18B20 temperature sensor driver for STM32F103
 * 
 * This driver implements a strictly non-blocking interface for the DS18B20 
 * temperature sensor using hardware timers and DMA on STM32F103 microcontrollers.
 * 
 * Key features:
 * - Pure bare-metal, register-level programming
 * - No interrupts, no software delays, no busy-waits
 * - Hardware timer-based timing with DMA for data capture
 * - Non-blocking state machine architecture
 * - Weak function callbacks for customization
 * 
 * Usage:
 * 1. Call ds18b20_init() once at startup
 * 2. Call ds18b20_poll() repeatedly from main loop
 * 3. Implement weak callbacks ds18b20_led_control() and ds18b20_temp_ready()
 *    to handle LED feedback and temperature results
 */

#pragma once

#include "stm32f0xx.h"

class DS18B20 {
    /**
     * @brief DS18B20 driver context structure using union for memory efficiency
     * @note Different stages of communication use the same memory for different purposes
     */
    typedef struct {
        union {
            volatile uint16_t edge[36];   /**< Edge timestamps for presence detection */
            volatile uint8_t pulse[72];   /**< Pulse durations for data decoding */
            uint8_t scratchpad[9];        /**< Sensor scratchpad data */
            uint64_t fill_union;          /**< Utility field for filling the union */
        };
        uint8_t current_state;            /**< Current state of the state machine */
    } DS18B20_ctx_t;

    DS18B20_ctx_t ctx;

    void ForceUpdateEvent(TIM_TypeDef *tim);

    uint8_t check_scratchpad_crc();

    void decode_scratchpad();

    int16_t decode_temperature();

    unsigned check_presence();

    void start_timer(uint16_t arr, uint8_t rcr);

    /**
     * @brief Wait for temperature conversion to complete (750ms typical)
     * @note Non-blocking - starts timer that will generate update event when complete
     */
    void wait_conversion() { start_timer(62500, 11); }

    /**
     * @brief Start inter-measurement pause period (500ms)
     * @note Non-blocking - starts timer for inter-measurement delay
     */
    void start_cycle_pause() { start_timer(62500, 79); }

    void reset_bus();

    void send_command(const uint8_t *cmd);

    void read_data();

public:
    /**
     * @brief Special error values (0.1°C units, outside -550..1250 range)
     * @note These values are outside the normal temperature range to indicate errors
     */
    enum ErrorStatus {
        TEMP_ERROR_GENERIC = INT16_MIN,      /**< Generic/unspecified error */
        TEMP_ERROR_NO_SENSOR,                /**< No sensor detected on bus */
        TEMP_ERROR_CRC_FAIL                  /**< CRC checksum validation failed */
    };

    /**
     * @brief Initialize DS18B20 driver hardware and peripherals
     */
    void init();

    /**
     * @brief Advance the state machine (non-blocking)
     * @note Call periodically from main loop
     *
     * This function implements the core non-blocking state machine that manages
     * the 1-Wire communication protocol with the DS18B20 sensor. It uses hardware
     * timer and DMA to handle timing-critical operations without software delays.
     */
    void poll();
};