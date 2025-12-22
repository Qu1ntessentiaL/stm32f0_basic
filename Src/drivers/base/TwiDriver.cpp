#include "TwiDriver.hpp"
#include "GpioDriver.hpp"

TwiDriver::Context TwiDriver::m_ctx{};
etl::circular_buffer<TwiDriver::Request, TwiDriver::QueueSize> TwiDriver::m_queue{};

void TwiDriver::init(uint32_t pclk1, uint32_t speedHz) {
    GpioDriver scl(GPIOB, 6);
    GpioDriver sda(GPIOB, 7);

    scl.Init(GpioDriver::Mode::Alternate);
    scl.SetAlternateFunction(1);

    sda.Init(GpioDriver::Mode::Alternate);
    sda.SetAlternateFunction(1);

    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;
    I2C1->CR1 &= ~I2C_CR1_PE; // Disable I2C before config

    // Configure timing
    uint32_t timing = 0;

    if (speedHz <= 100'000) {
        // Standard mode (100 kHz)
        if (pclk1 == 8'000'000) {
            timing =
                (1 << I2C_TIMINGR_PRESC_Pos) |
                (4 << I2C_TIMINGR_SCLDEL_Pos) |
                (2 << I2C_TIMINGR_SDADEL_Pos) |
                (15 << I2C_TIMINGR_SCLH_Pos) |
                (19 << I2C_TIMINGR_SCLL_Pos);
        }
        else if (pclk1 == 48'000'000) {
            timing =
                (5 << I2C_TIMINGR_PRESC_Pos) |
                (9 << I2C_TIMINGR_SCLDEL_Pos) |
                (4 << I2C_TIMINGR_SDADEL_Pos) |
                (33 << I2C_TIMINGR_SCLH_Pos) |
                (39 << I2C_TIMINGR_SCLL_Pos);
        }
        else {
            // Неподдерживаемая частота
            return;
        }
    }
    else {
        // Fast mode (400 kHz)
        if (pclk1 == 8000000) {
            timing =
                (0 << I2C_TIMINGR_PRESC_Pos) |
                (3 << I2C_TIMINGR_SCLDEL_Pos) |
                (1 << I2C_TIMINGR_SDADEL_Pos) |
                (6 << I2C_TIMINGR_SCLH_Pos) |
                (9 << I2C_TIMINGR_SCLL_Pos);
        }
        else if (pclk1 == 48000000) {
            timing =
                (2 << I2C_TIMINGR_PRESC_Pos) |
                (9 << I2C_TIMINGR_SCLDEL_Pos) |
                (3 << I2C_TIMINGR_SDADEL_Pos) |
                (16 << I2C_TIMINGR_SCLH_Pos) |
                (21 << I2C_TIMINGR_SCLL_Pos);
        }
        else {
            // Неподдерживаемая частота
            return;
        }
    }

    I2C1->TIMINGR = timing;
    I2C1->CR1 = I2C_CR1_TXIE | I2C_CR1_RXIE | I2C_CR1_TCIE | I2C_CR1_ERRIE | I2C_CR1_PE;
    NVIC_EnableIRQ(I2C1_IRQn);
}

bool TwiDriver::submit(const Request &r) {
    if (m_queue.full())
        return false;

    m_queue.push(r);

    if (m_ctx.state == State::Idle)
        start_next();

    return true;
}

// Start next transaction

void TwiDriver::start_next() {
    if (m_queue.empty())
        return;

    m_ctx.req = m_queue.front();
    m_queue.pop();

    m_ctx.tx_pos = 0;
    m_ctx.rx_pos = 0;
    m_ctx.timeout = TimeoutMs;
    m_ctx.state = State::Start;

    uint8_t nbytes = m_ctx.req.tx_len ? m_ctx.req.tx_len : m_ctx.req.rx_len;

    I2C1->CR2 =
        (m_ctx.req.address << 1) |
        (nbytes << I2C_CR2_NBYTES_Pos) |
        (m_ctx.req.rx_len ? I2C_CR2_RD_WRN : 0) |
        I2C_CR2_START;
}

// IRQ FSM

void TwiDriver::irq() {
    uint32_t isr = I2C1->ISR;

    if (isr & (I2C_ISR_NACKF | I2C_ISR_BERR)) {
        m_ctx.state = State::Error;
    }

    switch (m_ctx.state) {
    case State::Tx:
        if (isr & I2C_ISR_TXIS) {
            I2C1->TXDR = m_ctx.req.tx[m_ctx.tx_pos++];

            if (m_ctx.tx_pos >= m_ctx.req.tx_len) {
                if (m_ctx.req.rx_len) {
                    m_ctx.state = State::Start;
                    m_ctx.timeout = TimeoutMs;

                    I2C1->CR2 =
                        (m_ctx.req.address << 1) |
                        (m_ctx.req.rx_len << I2C_CR2_NBYTES_Pos) |
                        I2C_CR2_RD_WRN |
                        I2C_CR2_START;
                }
                else {
                    m_ctx.state = State::Stop;
                }
            }
        }
        break;

    case State::Rx:
        if (isr & I2C_ISR_RXNE) {
            m_ctx.req.rx[m_ctx.rx_pos++] = I2C1->RXDR;

            if (m_ctx.rx_pos >= m_ctx.req.rx_len)
                m_ctx.state = State::Stop;
        }
        break;

    case State::Stop:
        I2C1->CR2 |= I2C_CR2_STOP;
        finish(true);
        break;

    case State::Error:
        I2C1->CR2 |= I2C_CR2_STOP;
        finish(false);
        break;

    default:
        if (isr & I2C_ISR_TXIS)
            m_ctx.state = State::Tx;
        else if (isr & I2C_ISR_RXNE)
            m_ctx.state = State::Rx;
        break;
    }
}

// Timeout

void TwiDriver::tick_1ms() {
    if (m_ctx.state == State::Idle)
        return;

    if (m_ctx.timeout > 0) {
        m_ctx.timeout--;
        if (m_ctx.timeout == 0)
            m_ctx.state = State::Error;
    }
}

// Finish

void TwiDriver::finish(bool ok) {
    if (m_ctx.req.callback)
        m_ctx.req.callback(ok);

    m_ctx.state = State::Idle;
    start_next();
}

// IRQ entry

extern "C" void I2C1_IRQHandler() {
    TwiDriver::irq();
}
