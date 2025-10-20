#include "TwiDriver.hpp"

void TwiDriver::init(uint32_t speed) {
    RCC->AHBENR |= RCC_AHBENR_GPIOBEN;
    RCC->APB1ENR |= RCC_APB1ENR_I2C1EN;

    // PB6=SCL, PB7=SDA (AF1)
    GPIOB->MODER &= ~(GPIO_MODER_MODER6 | GPIO_MODER_MODER7);
    GPIOB->MODER |= (GPIO_MODER_MODER6_1 | GPIO_MODER_MODER7_1);
    GPIOB->OTYPER |= GPIO_OTYPER_OT_6 | GPIO_OTYPER_OT_7;
    GPIOB->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR6 | GPIO_OSPEEDER_OSPEEDR7;
    GPIOB->PUPDR &= ~(GPIO_PUPDR_PUPDR6 | GPIO_PUPDR_PUPDR7);
    GPIOB->AFR[0] |= (1 << (6 * 4)) | (1 << (7 * 4));

    I2C1->CR1 = I2C_CR1_SWRST;
    I2C1->CR1 = 0;
    I2C1->TIMINGR = (1 << 28) | (4 << 20) | (2 << 16) | (0xF << 8) | (0x13);
    I2C1->CR1 |= I2C_CR1_PE;
}

bool TwiDriver::waitFlag(uint32_t flag, bool set, uint32_t timeout) {
    while (((I2C1->ISR & flag) != 0) != set) {
        if (--timeout == 0) return false;
    }
    return true;
}

bool TwiDriver::start(uint8_t addr, bool read, uint8_t nbytes) {
    if (!waitFlag(I2C_ISR_BUSY, false)) return false;
    I2C1->CR2 = (addr << 1) | (nbytes << 16);
    if (read) I2C1->CR2 |= I2C_CR2_RD_WRN;
    I2C1->CR2 |= I2C_CR2_START;
    return true;
}

bool TwiDriver::stop() {
    I2C1->CR2 |= I2C_CR2_STOP;
    return waitFlag(I2C_ISR_STOPF, true);
}

bool TwiDriver::waitTX() { return waitFlag(I2C_ISR_TXIS, true); }

bool TwiDriver::waitRX() { return waitFlag(I2C_ISR_RXNE, true); }

bool TwiDriver::waitTC() { return waitFlag(I2C_ISR_TC, true); }

bool TwiDriver::waitSTOP() { return waitFlag(I2C_ISR_STOPF, true); }

bool TwiDriver::sendByte(uint8_t data) {
    if (!waitTX()) return false;
    I2C1->TXDR = data;
    return true;
}

uint8_t TwiDriver::readByte() {
    waitRX();
    return static_cast<uint8_t>(I2C1->RXDR);
}

bool TwiDriver::write(uint8_t addr, const uint8_t *data, size_t len) {
    if (!start(addr, false, len)) return false;
    for (size_t i = 0; i < len; ++i)
        if (!sendByte(data[i])) return false;
    if (!waitTC()) return false;
    return stop();
}

bool TwiDriver::read(uint8_t addr, uint8_t *data, size_t len) {
    if (!start(addr, true, len)) return false;
    for (size_t i = 0; i < len; ++i)
        data[i] = readByte();
    if (!waitTC()) return false;
    return stop();
}

bool TwiDriver::writeRead(uint8_t addr, const uint8_t *tx, size_t tx_len, uint8_t *rx, size_t rx_len) {
    if (!start(addr, false, tx_len)) return false;
    for (size_t i = 0; i < tx_len; ++i)
        if (!sendByte(tx[i])) return false;
    if (!waitTC()) return false;
    // Повторный START
    if (!start(addr, true, rx_len)) return false;
    for (size_t i = 0; i < rx_len; ++i)
        rx[i] = readByte();
    if (!waitTC()) return false;
    return stop();
}

