#pragma once

#include <etl/circular_buffer.h>
#include <etl/string.h>

#include "stm32f0xx.h"
#include "GpioDriver.hpp"

/**
 * @brief Драйвер USART с неблокирующей передачей через кольцевой буфер
 * @tparam Baudrate Скорость передачи (по умолчанию 115200)
 * @tparam TxBuffSize Размер кольцевого буфера для передачи
 */
template<uint32_t Baudrate = 115200, size_t TxBuffSize = 256>
class UsartDriver {
    etl::circular_buffer<uint8_t, TxBuffSize> m_tx_buf; ///< Кольцевой буфер для передачи

    /**
     * @brief Включает тактирование USART1
     */
    static void EnableClock() {
        if (!(RCC->APB2ENR & RCC_APB2ENR_USART1EN)) {
            RCC->APB2ENR |= RCC_APB2ENR_USART1EN;
            __DSB();
        }
    }

    /**
     * @brief Выключает тактирование USART1
     */
    static void DisableClock() {
        if (RCC->APB2ENR & RCC_APB2ENR_USART1EN) {
            RCC->APB2ENR &= ~RCC_APB2ENR_USART1EN;
            __DSB();
        }
    }

    /**
     * @brief Неблокирующая запись одного байта в буфер передачи
     * @param byte Байта для записи
     * @return true, если запись успешна, false если буфер полон
     * @note Включает прерывание TXE при успешной записи
     */
    bool tx_enqueue_byte(uint8_t byte) {
        if (m_tx_buf.full()) {
            return false;
        }
        m_tx_buf.push(byte);
        USART1->CR1 |= USART_CR1_TXEIE;
        return true;
    }

public:
    /**
     * @brief Инициализация USART1 и GPIO
     * @param apb_clk_hz Частота шины APB (Гц)
     */
    static void Init(uint32_t apb_clk_hz) {
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
        USART1->CR1 |= USART_CR1_UE; // Включение USART

        NVIC_EnableIRQ(USART1_IRQn);
    }

    /**
     * @brief Неблокирующая запись C-style строки в буфер передачи
     * @param s Указатель на нуль-терминированную строку
     * @return Количество успешно записанных символов
     */
    int write_str(const char *s) {
        int count = 0;
        while (*s && tx_enqueue_byte(static_cast<uint8_t>(*s))) {
            s++;
            count++;
        }
        return count;
    }

    /**
     * @brief Неблокирующая запись etl::string в буфер передачи
     * @tparam N Максимальный размер строки etl::string
     * @param s Строка для передачи
     * @return Количество успешно записанных символов
     */
    template<size_t N>
    int write_str(const etl::string<N> &s) {
        int count = 0;
        for (auto c : s) {
            if (!tx_enqueue_byte(static_cast<uint8_t>(c))) break;
            count++;
        }
        return count;
    }

    /**
     * @brief Конвертирует целое число в строку и записывает в буфер передачи
     * @param value Целое число для передачи
     */
    void write_int(int value) {
        etl::string<7> buf;
        buf.clear();

        if (value == 0) {
            buf.push_back('0');
        } else {
            int is_negative = 0;
            unsigned int uvalue = value;
            if (value < 0) {
                is_negative = 1;
                uvalue = -value;
            }

            etl::string<7> tmp;
            while (uvalue) {
                tmp.push_back('0' + (uvalue % 10));
                uvalue /= 10;
            }

            if (is_negative) tmp.push_back('-');

            for (int i = tmp.size() - 1; i >= 0; --i) {
                buf.push_back(tmp[i]);
            }
        }

        write_str(buf);
    }

    /**
     * @brief Обработчик прерывания USART1
     * @note Должен вызываться из ISR (например, USART1_IRQHandler)
     * @note Передача байтов из кольцевого буфера осуществляется здесь
     */
    void handleIRQ() {
        if (USART1->ISR & USART_ISR_TXE && !m_tx_buf.empty()) {
            USART1->TDR = m_tx_buf.front();
            m_tx_buf.pop();
        }

        if (m_tx_buf.empty()) {
            USART1->CR1 &= ~USART_CR1_TXEIE;
        }
    }
};