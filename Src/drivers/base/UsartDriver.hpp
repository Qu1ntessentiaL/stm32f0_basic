#pragma once

#include <etl/circular_buffer.h>
#include <etl/string.h>

#include "stm32f0xx.h"
#include "GpioDriver.hpp"

/**
 * @brief Драйвер USART с неблокирующей передачей и приемом через кольцевые буферы
 * @tparam Baudrate Скорость передачи (по умолчанию 115200)
 * @tparam TxBuffSize Размер кольцевого буфера для передачи
 * @tparam RxBuffSize Размер кольцевого буфера для приема
 */
template<uint32_t Baudrate = 115200, size_t TxBuffSize = 256, size_t RxBuffSize = 256>
class UsartDriver {
    etl::circular_buffer<uint8_t, TxBuffSize> m_tx_buf; ///< Кольцевой буфер для передачи
    etl::circular_buffer<uint8_t, RxBuffSize> m_rx_buf; ///< Кольцевой буфер для приема
    volatile uint32_t m_rx_overrun_count = 0;            ///< Счетчик переполнений буфера приема

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
        
        // Включение прерываний на прием (RXNE) и передачу (TXE)
        USART1->CR1 |= USART_CR1_RXNEIE;

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
     * @note Обрабатывает передачу (TXE) и прием (RXNE) данных
     */
    void handleIRQ() {
        // Обработка передачи (TXE - Transmit Data Register Empty)
        if (USART1->ISR & USART_ISR_TXE) {
            if (!m_tx_buf.empty()) {
                USART1->TDR = m_tx_buf.front();
                m_tx_buf.pop();
            } else {
                // Буфер пуст - отключаем прерывание TXE
                USART1->CR1 &= ~USART_CR1_TXEIE;
            }
        }

        // Обработка приема и ошибок
        // Важно: проверяем ORE перед RXNE, так как чтение RDR очищает оба флага
        uint32_t isr = USART1->ISR;
        
        // Обработка ошибки переполнения (ORE - Overrun Error)
        // ORE возникает когда новые данные приходят до чтения предыдущих из RDR
        if (isr & USART_ISR_ORE) {
            // Читаем RDR для очистки флагов ORE и RXNE
            // Данные теряются на уровне железа (не успели прочитать вовремя)
            volatile uint8_t dummy = static_cast<uint8_t>(USART1->RDR);
            (void)dummy;
            ++m_rx_overrun_count;
        }
        // Обработка приема (RXNE - Read Data Register Not Empty)
        // Проверяем только если ORE не был установлен (чтение RDR выше очистило бы RXNE)
        else if (isr & USART_ISR_RXNE) {
            // Читаем данные из регистра (это также очищает флаг RXNE)
            uint8_t byte = static_cast<uint8_t>(USART1->RDR);
            
            if (!m_rx_buf.full()) {
                m_rx_buf.push(byte);
            } else {
                // Буфер переполнен - инкрементируем счетчик переполнений
                // Данные теряются (FIFO поведение - старые данные сохраняются)
                ++m_rx_overrun_count;
            }
        }
        
        // Примечание: флаги FE (Frame Error), PE (Parity Error), NE (Noise Error)
        // очищаются автоматически при чтении RDR вместе с RXNE
        // Если нужна детальная диагностика ошибок, можно добавить отдельные счетчики
    }

    /**
     * @brief Получить количество доступных байт в буфере приема
     * @return Количество байт, доступных для чтения
     */
    size_t available() const {
        return m_rx_buf.size();
    }

    /**
     * @brief Проверить, есть ли данные для чтения
     * @return true, если есть данные для чтения
     */
    bool has_data() const {
        return !m_rx_buf.empty();
    }

    /**
     * @brief Прочитать один байт из буфера приема
     * @param[out] byte Указатель на переменную для записи байта
     * @return true, если байт успешно прочитан, false если буфер пуст
     */
    bool read(uint8_t *byte) {
        if (m_rx_buf.empty()) {
            return false;
        }
        *byte = m_rx_buf.front();
        m_rx_buf.pop();
        return true;
    }

    /**
     * @brief Прочитать один байт из буфера приема
     * @return Прочитанный байт или -1 если буфер пуст
     */
    int read_byte() {
        if (m_rx_buf.empty()) {
            return -1;
        }
        uint8_t byte = m_rx_buf.front();
        m_rx_buf.pop();
        return static_cast<int>(byte);
    }

    /**
     * @brief Прочитать строку из буфера приема до терминатора или максимум N байт
     * @tparam N Максимальный размер строки etl::string
     * @param[out] str Строка для записи результата
     * @param terminator Терминатор (по умолчанию '\n')
     * @return Количество прочитанных байт (включая терминатор, если найден)
     */
    template<size_t N>
    size_t read_str(etl::string<N> &str, char terminator = '\n') {
        str.clear();
        size_t count = 0;
        
        while (!m_rx_buf.empty() && str.size() < str.max_size() - 1) {
            uint8_t byte = m_rx_buf.front();
            m_rx_buf.pop();
            ++count;
            
            str.push_back(static_cast<char>(byte));
            
            if (static_cast<char>(byte) == terminator) {
                break;
            }
        }
        
        return count;
    }

    /**
     * @brief Прочитать C-style строку из буфера приема
     * @param[out] buffer Буфер для записи строки
     * @param buffer_size Размер буфера (включая нуль-терминатор)
     * @param terminator Терминатор (по умолчанию '\n')
     * @return Количество прочитанных байт (включая терминатор, если найден)
     */
    size_t read_str(char *buffer, size_t buffer_size, char terminator = '\n') {
        if (buffer == nullptr || buffer_size == 0) {
            return 0;
        }
        
        size_t count = 0;
        size_t max_read = buffer_size - 1; // Оставляем место для '\0'
        
        while (!m_rx_buf.empty() && count < max_read) {
            uint8_t byte = m_rx_buf.front();
            m_rx_buf.pop();
            
            buffer[count] = static_cast<char>(byte);
            ++count;
            
            if (static_cast<char>(byte) == terminator) {
                break;
            }
        }
        
        buffer[count] = '\0';
        return count;
    }

    /**
     * @brief Очистить буфер приема
     */
    void clear_rx() {
        m_rx_buf.clear();
    }

    /**
     * @brief Получить счетчик переполнений буфера приема
     * @return Количество переполнений с момента инициализации
     * @note Счетчик инкрементируется при попытке записи в полный буфер
     */
    uint32_t get_rx_overrun_count() const {
        return m_rx_overrun_count;
    }

    /**
     * @brief Сбросить счетчик переполнений буфера приема
     */
    void reset_rx_overrun_count() {
        m_rx_overrun_count = 0;
    }
};