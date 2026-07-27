#pragma once

#include "stm32f0xx.h"
#include "GpioDriver.hpp"

/**
 * @brief Драйвер LCD-контроллера HT1621B.
 *
 * Плата: CS=PB5, WR=PB4, DATA=PB3.
 *
 * Дисплей: 6 семисегментных разрядов + десятичные точки + иконки
 * (уровень заряда, NET, «k», «g» и др.). Каждый разряд занимает 4 адреса VRAM
 * (base+1 .. base+4); позиция 0 — правый разряд, 5 — левый.
 *
 * Таблицы глифов (цифры, буквы, точки, иконки) хранятся в ht1621.cpp
 * в компактном виде: 4 байта на символ (значения нибблов base+1..base+4).
 *
 * Обновление экрана двухфазное:
 *  1. Высокоуровневые Show* меняют локальный m_vram и помечают адреса в m_dirty.
 *  2. Flush() отправляет на HT1621 только изменённые (или все) адреса.
 */
class HT1621B {
    /** Размер RAM контроллера HT1621B (32 × 4 бита). */
    static constexpr uint8_t kVramSize = 32;

    /** Количество семисегментных разрядов на дисплее. */
    static constexpr uint8_t kDigitCount = 6;

    /** Число VRAM-адресов на один разряд (4 сегментных ниббла). */
    static constexpr uint8_t kSegsPerDigit = 4;

    uint8_t m_vram[kVramSize]{}; ///< Зеркало RAM контроллера HT1621B
    uint32_t m_dirty = 0; ///< Бит N = 1 → адрес N нужно отправить в Flush()

    GpioDriver m_cs_pin;   ///< CS  (PB5)
    GpioDriver m_write_pin; ///< WR  (PB4)
    GpioDriver m_data_pin;  ///< DATA (PB3)

    /**
     * @brief Программная задержка в циклах CPU.
     * @param n Число итераций __NOP (калибровано под 48 МГц)
     */
    void delayCycles(uint32_t n) const;

    /**
     * @brief Запись одного бита данных или команды в контроллер HT1621B.
     * @param bit Значение бита (0 или 1)
     */
    void writeBit(bool bit) const;

    /**
     * @brief Отправка команды на контроллер HT1621B.
     * @param cmd Код команды (см. datasheet, enum Command в ht1621.cpp)
     */
    void writeCommand(uint8_t cmd);

    /**
     * @brief Запись непрерывного диапазона адресов VRAM за одну транзакцию.
     *
     * HT1621 после указания стартового адреса автоматически инкрементирует его
     * при последовательной передаче 4-битных слов.
     *
     * @param startAddr Начальный адрес VRAM [0..31]
     * @param endAddr   Конечный адрес VRAM (включительно)
     */
    void writeDataBurst(uint8_t startAddr, uint8_t endAddr);

    /** @brief Отправить весь m_vram[0..31] одной транзакцией. */
    void flushAll();

    /**
     * @brief Отправить только изменённые адреса (см. m_dirty).
     *
     * Сливает соседние «грязные» адреса в один burst. Если изменено больше
     * половины RAM — вызывает flushAll() как более выгодный вариант.
     */
    void flushDirty();

    /**
     * @brief Записать значение в VRAM и пометить адрес, если байт изменился.
     * @param addr  Адрес VRAM [0..31]
     * @param value Новое 4-битное значение
     */
    void touch(uint8_t addr, uint8_t value);

    /**
     * @brief Установить биты в VRAM (OR).
     * @param addr Адрес VRAM
     * @param mask Маска устанавливаемых битов
     */
    void setBits(uint8_t addr, uint8_t mask);

    /**
     * @brief Сбросить биты в VRAM (AND NOT).
     * @param addr Адрес VRAM
     * @param mask Маска сбрасываемых битов
     */
    void clearBits(uint8_t addr, uint8_t mask);

    /**
     * @brief Вывести глиф (цифру или букву) в заданный разряд.
     *
     * Записывает 4 ниббла разряда, не затрагивая биты DP и иконки «k»
     * в общем ниббле base+3 (маска 0x01 вместо 0x03).
     *
     * @param base Базовый адрес разряда: (5 − position) × 4
     * @param segs Указатель на 4 байта — значения нибблов base+1..base+4
     *             (таблицы kDigitGlyphs / kLetterGlyphs в ht1621.cpp)
     */
    void writeGlyph(uint8_t base, const uint8_t segs[kSegsPerDigit]);

    /**
     * @brief Индекс буквы в kLetterGlyphs или −1, если символ не поддержан.
     * @param c Символ из набора: A, b, C, d, E, F, G, h, I, J, L, n, o, P, r, t, U, X, -, _, пробел
     */
    static int letterIndex(char c);

    /** @brief Вывести символ (цифра или буква) в разряд position. */
    void showChar(uint8_t position, char c);

    /** @brief Начать SPI-подобную транзакцию (CS↓, преамбула 100/0/D). */
    void beginTransfer(bool isData) const;

    /** @brief Завершить транзакцию (CS↑). */
    void endTransfer() const;

    /** @brief Задержка в микросекундах (калибровка под 48 МГц). */
    void delayUs(uint32_t us) const;

    /**
     * @brief Базовый VRAM-адрес разряда по его позиции на дисплее.
     * @param position Позиция [0..5]: 0 — правый, 5 — левый
     * @return (5 − position) × kSegsPerDigit
     */
    static uint8_t digitBase(uint8_t position) {
        return static_cast<uint8_t>((kDigitCount - 1 - position) * kSegsPerDigit);
    }

public:
    /** Иконки на дисплее (см. kSpecials в ht1621.cpp). */
    enum class Special : uint8_t {
        Arrow = 0, ///< «->0<-»
        Net = 1,   ///< «NET»
        K = 2,     ///< «k»
        G = 3,     ///< «g»
    };

    /**
     * @brief Конструктор: инициализация GPIO (PB3..PB5) и установка шины в idle.
     *
     * Не вызывает Init() — команды HT1621 отправляются отдельно из hardware_init().
     */
    HT1621B();

    /**
     * @brief Инициализация контроллера HT1621B.
     *
     * Последовательность: RC256K → Bias 1/2 (4 commons) → SysEn → LcdOn → FullClear.
     */
    void Init();

    /**
     * @brief Немедленно вывести изменённые ячейки VRAM на дисплей.
     *
     * Рекомендуется вызывать один раз после серии Show* с flushNow = false.
     */
    void Flush();

    /**
     * @brief Полностью очистить VRAM (все адреса → 0).
     * @param flushNow Немедленно вывести результат на дисплей
     */
    void FullClear(bool flushNow = false);

    /**
     * @brief Очистить область VRAM, соответствующую только сегментным индикаторам.
     *
     * Адреса 1..24 (6 разрядов × 4 ниббла). Адрес 0x17 очищается частично:
     * сбрасывается только бит DP, иконка «k» (бит 1) сохраняется.
     *
     * @param flushNow Немедленно вывести результат на дисплей
     */
    void ClearSegArea(bool flushNow = false);

    /**
     * @brief Отобразить или погасить десятичный разделитель.
     * @param position Позиция разделителя [1..5] (0 недопустима — нет точки справа от младшего разряда)
     * @param enable   true — зажечь, false — погасить
     * @param flushNow Немедленно вывести результат на дисплей
     */
    void ShowDot(uint8_t position, bool enable, bool flushNow = false);

    /**
     * @brief Отобразить или скрыть специальный символ (иконку) на дисплее.
     * @param type     Иконка (см. HT1621B::Special)
     * @param enable   true — показать, false — скрыть
     * @param flushNow Немедленно вывести результат на дисплей
     */
    void ShowSpecial(Special type, bool enable, bool flushNow = false);

    /**
     * @brief Заполнить все ячейки VRAM значением 0x0F (все сегменты включены).
     * @param flushNow Немедленно вывести результат на дисплей
     */
    void ShowFull(bool flushNow = false);

    /**
     * @brief Вывести предопределённую букву на заданной позиции дисплея.
     * @param position Позиция символа [0..5]: 0 — правый, 5 — левый
     * @param c        Символ из набора kLetterGlyphs (см. letterIndex)
     * @param flushNow Немедленно вывести результат на дисплей
     */
    void ShowLetter(uint8_t position, char c, bool flushNow = false);

    /**
     * @brief Вывести строку из цифр и букв (не более 6 символов) на дисплей.
     *
     * Перед выводом очищает сегментную область (ClearSegArea), не трогая иконки.
     * Строка выводится справа налево: первый символ str — левый разряд.
     *
     * @param str      Строка: цифры '0'..'9', буквы из kLetterGlyphs, пробел
     * @param flushNow Немедленно вывести результат на дисплей
     */
    void ShowString(const char *str, bool flushNow = false);

    /**
     * @brief Вывести целое число на дисплей (не более 6 цифр, с минусом — 5).
     * @param value    Выводимое число; при переполнении показывается «------»
     * @param flushNow Немедленно вывести результат на дисплей
     */
    void ShowInt(int value, bool flushNow = false);

    /**
     * @brief Вывести одну цифру на заданной позиции дисплея.
     * @param position Позиция [0..5]: 0 — правый разряд, 5 — левый
     * @param digit    Цифра 0..9
     * @param withDot  true — зажечь десятичную точку слева от разряда
     * @param flushNow Немедленно вывести результат на дисплей
     */
    void ShowDigit(uint8_t position, uint8_t digit, bool withDot, bool flushNow = false);

    /**
     * @brief Отобразить символ уровня заряда батареи.
     * @param level    Уровень [0..3]; значения > 3 трактуются как 3
     * @param flushNow Немедленно вывести результат на дисплей
     */
    void ShowChargeLevel(uint8_t level, bool flushNow = false);

    /**
     * @brief Вывести дату на дисплей в формате DD.MM.YY.
     * @param day      День [1..31]
     * @param month    Месяц [1..12]
     * @param year     Год [00..99]
     * @param flushNow Немедленно вывести результат на дисплей
     */
    void ShowDate(uint8_t day, uint8_t month, uint8_t year, bool flushNow = false);
};
