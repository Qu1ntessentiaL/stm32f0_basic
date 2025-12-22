#pragma once

#include "stm32f0xx.h"
#include "etl/circular_buffer.h"

class TwiDriver {
public:
    /**
     * @brief Описание транзакций
     */
    struct Request {
        uint8_t address;
        const uint8_t *tx;
        uint8_t tx_len;
        uint8_t *rx;
        uint8_t rx_len;
        void (*callback)(bool ok);
    };

    static void init(uint32_t pclk1 = 48'000'000, uint32_t speedHz = 100'000);
    static bool submit(const Request &r);
    static void irq();
    static void tick_1ms();

private:
    /**
     * @brief Состояния FSM
     */
    enum class State : uint8_t {
        Idle,
        Start,
        Tx,
        Rx,
        Stop,
        Error
    };

    /**
     * @brief Контекст FSM
     */
    struct Context {
        State state = State::Idle;
        Request req{};
        uint8_t tx_pos = 0;
        uint8_t rx_pos = 0;
        uint16_t timeout = 0;
    };

    static constexpr uint8_t TimeoutMs = 100;

    static Context m_ctx;

    static constexpr size_t QueueSize = 4;
    static etl::circular_buffer<Request, QueueSize> m_queue;

    static void start_next();
    static void finish(bool ok);
};
