#pragma once

#include "stm32f0xx.h"
#include "GpioDriver.hpp"

class HT1621B {
    struct Segment {
        uint8_t addr;
        uint8_t val;
    };

    enum class WriteMode : uint8_t {
        Replace,
        SetBit,
        ClearBit
    };

    /**
     * @brief Массив для формирования цифр
     */
    static constexpr Segment m_digits[10][4] = {
            /* 0 */ {{1, 3}, {2, 2}, {3, 1}, {4, 3}},
            /* 1 */ {{1, 0}, {2, 0}, {3, 1}, {4, 2}},
            /* 2 */ {{1, 2}, {2, 3}, {3, 0}, {4, 3}},
            /* 3 */ {{1, 0}, {2, 3}, {3, 1}, {4, 3}},
            /* 4 */ {{1, 1}, {2, 1}, {3, 1}, {4, 2}},
            /* 5 */ {{1, 1}, {2, 3}, {3, 1}, {4, 1}},
            /* 6 */ {{1, 3}, {2, 3}, {3, 1}, {4, 1}},
            /* 7 */ {{1, 0}, {2, 0}, {3, 1}, {4, 3}},
            /* 8 */ {{1, 3}, {2, 3}, {3, 1}, {4, 3}},
            /* 9 */ {{1, 1}, {2, 3}, {3, 1}, {4, 3}}
    };

    /**
     * @brief Набор латинских букв для 7-сегментного индикатора
     */
    static constexpr Segment m_letters[][4] = {
            /* A */ {{1,3}, {2,1}, {3,1}, {4,3}},
            /* b */ {{1,3}, {2,3}, {3,1}, {4,0}},
            /* C */ {{1,3}, {2,2}, {3,0}, {4,1}},
            /* d */ {{1,2}, {2,3}, {3,1}, {4,2}},
            /* E */ {{1,3}, {2,3}, {3,0}, {4,1}},
            /* F */ {{1,3}, {2,1}, {3,0}, {4,1}},
            /* G */ {{1,3}, {2,2}, {3,1}, {4,1}},
            /* h */ {{1,3}, {2,1}, {3,1}, {4,0}},
            /* I */ {{1,3}, {2,0}, {3,0}, {4,0}},
            /* J */ {{1,0}, {2,2}, {3,1}, {4,2}},
            /* L */ {{1,3}, {2,2}, {3,0}, {4,0}},
            /* n */ {{1,2}, {2,1}, {3,1}, {4,0}},
            /* o */ {{1,2}, {2,3}, {3,1}, {4,0}},
            /* P */ {{1,3}, {2,1}, {3,0}, {4,3}},
            /* r */ {{1,2}, {2,1}, {3,0}, {4,0}},
            /* t */ {{1,3}, {2,3}, {3,0}, {4,0}},
            /* U */ {{1,3}, {2,2}, {3,1}, {4,2}},
            /* X */ {{1,3}, {2,1}, {3,1}, {4,2}},
            /* - */ {{1,0}, {2,1}, {3,0}, {4,0}},
            /* _ */ {{1,0}, {2,2}, {3,0}, {4,0}},
            /* Space {{1,0}, {2,0}, {3,0}, {4,0}}, */
    };

    /**
     * @brief Массив для формирования десятичных разделителей
     */
    static constexpr Segment m_dots[5] = {
            {0x03, 0x02},
            {0x07, 0x02},
            {0x0B, 0x02},
            {0x0F, 0x02},
            {0x13, 0x02},
    };

    /**
     * @brief Массив для формирования знаков уровня заряда
     */
    static constexpr Segment m_chargeLevels[4] = {
            {0x1A, 1},
            {0x1B, 2},
            {0x1B, 1},
            {0x1A, 2},
    };

    /**
     * @brief Массив для формирования специальных символов
     */
    static constexpr Segment m_specials[4] = {
            /* ->0<- */ {0x00, 0x01},
            /*  NET  */ {0x00, 0x02},
            /*   k   */ {0x17, 0x02},
            /*   g   */ {0x19, 0x02},
    };

    /**
     * @brief Управляющие команды для контроллера HT1621B
     */
    enum Commands : uint8_t {
        SysDis = 0,
        SysEn,
        LcdOff,
        LcdOn,
        RC256K = 0x18,
        Bias12 = 0x28, // 4 commons option
        Bias13 = 0x29, // 4 commons option
    };

    uint8_t m_vram[32] = {0}; ///< RAM-память для контроллера HT1621B

    GpioDriver m_cs_pin, m_write_pin, m_data_pin;

    /**
     * @brief Запись бита данных или команды в контроллер HT1621B
     * @param bit Бит данных или команды
     */
    void WriteBit(uint8_t bit);

    /**
     * @brief Отправка команды на контроллер HT1621B
     * @param cmd Команда контроллера HT1621B (см. datasheet)
     */
    void WriteCommand(Commands cmd);

    /**
     * @brief Отправляет данные на дисплей
     * @param address
     * @param data Байт данных
     */
    void WriteData(uint8_t address, uint8_t data);

    /**
     * @brief Функция, записывающая данные от высокоуровневых функций в RAM
     * @param address
     * @param data
     * @param mode
     */
    void SetData(uint8_t address, uint8_t data, WriteMode mode = WriteMode::Replace);

public:
    HT1621B();

    /**
     * Немедленно выводит содержимое RAM на дисплей (используя функцию WriteData)
     */
    void Flush();

    /**
     * @brief Полностью очищает RAM
     * @param flushNow Немедленно выводит результат операции на дисплей
     */
    void FullClear(bool flushNow = false);

    /**
     * @brief Очищает область RAM, соответствующую только сегментным индикаторам
     * @param flushNow Немедленно выводит результат операции на дисплей
     */
    void ClearSegArea(bool flushNow = false);

    /**
     * @brief Инициализация дисплея
     */
    void Init();

    /**
     * @brief Отображает десятичный разделитель в заданной позиции
     * @param position Позиция десятичного разделителя [5..0] (0 - правый разряд, 5 - левый)
     * @param enable Отобразить или погасить разделитель
     * @param flushNow Немедленно выводит результат операции на дисплей
     */
    void ShowDot(uint8_t position, bool enable, bool flushNow = false);

    /**
     * @brief Позволяет отображать или скрывать специальные символы на дисплее
     * @param type [0..3] - индекс массива m_specials
     *             0 - "->0<-"
     *             1 - "NET"
     *             2 - "k"
     *             3 - "g"
     * @param enable Отобразить или погасить спецсимвол
     * @param flushNow Немедленно выводит результат операции на дисплей
     */
    void ShowSpecial(uint8_t type, bool enable, bool flushNow = false);

    /**
     * @brief Заполняет все биты RAM единицами
     * @param flushNow Немедленно выводит результат операции на дисплей
     */
    void ShowFull(bool flushNow = false);

    /**
     * @brief Вывод предопределенного символа на заданной позиции дисплея
     * @param position Позиция символа [5..0] (0 - правый разряд, 5 - левый)
     * @param c Символ из набора m_letters
     * @param flushNow Немедленно выводит результат операции на дисплей
     */
    void ShowLetter(uint8_t position, char c, bool flushNow = false);

    /**
     * @brief Выводит строку из m_digits и m_letters (не более 6 символов) на дисплей
     * @param str Строка, содержащая m_digits, m_letters или пробел
     * @param flushNow Немедленно выводит результат операции на дисплей
     */
    void ShowString(const char *str, bool flushNow = false);

    /**
     * @brief Выводит целое число на дисплей (не более 6 цифр)
     * @param value Выводимое число
     * @param flushNow Немедленно выводит результат операции на дисплей
     */
    void ShowInt(int value, bool flushNow = false);

    /**
     * @brief Выводит цифру на заданной позиции дисплея
     * @param position Позиция цифры [5..0] (0 - правый разряд, 5 - левый)
     * @param digit Цифра 0 - 9
     * @param withDot Выводит десятичный разделитель
     * @param flushNow Немедленно выводит результат операции на дисплей
     */
    void ShowDigit(uint8_t position, uint8_t digit, bool withDot, bool flushNow = false);

    /**
     * @brief Отображает символ уровня заряда на дисплее
     * @param level Уровень заряда [0..3]
     * @param flushNow Немедленно выводит результат операции на дисплей
     */
    void ShowChargeLevel(uint8_t level, bool flushNow = false);

    /**
     * @brief Выводит дату на дисплей в формате xx.xx.xx
     * @param day День [1..31]
     * @param month Месяц [1..12]
     * @param year Год [00..99]
     * @param flushNow Немедленно выводит результат операции на дисплей
     */
    void ShowDate(uint8_t day, uint8_t month, uint8_t year, bool flushNow = false);
};