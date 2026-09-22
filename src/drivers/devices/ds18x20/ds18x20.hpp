/**
 * @file ds18x20.hpp
 * @brief Неблокирующий драйвер Dallas/Maxim DS18B20 и DS18S20 для STM32F0.
 *
 * @par Железо
 * Шина 1-Wire на PA8 (TIM1_CH1, AF2, open-drain). Тайминги бит-слота и reset
 * формирует TIM1 в one-pulse mode; DMA1_CH3 захватывает presence/данные,
 * DMA1_CH4 подгружает длительности импульсов при передаче команды.
 *
 * @par Протокол
 * - Convert T (`0xCC 0x44`) — Skip ROM, все датчики считают температуру сразу.
 * - Чтение scratchpad — по слотам таблицы @c DS18X20_SENSORS из config.h:
 *   Match ROM (`0x55` + 8 байт ROM + `0xBE`), если ROM задан;
 *   Skip ROM (`0xCC 0xBE`) только при одном слоте с нулевым ROM.
 * Номер датчика = индекс в таблице. Это стабильный идентификатор, в отличие
 * от порядка Search ROM.
 *
 * @par Асинхронность
 * @ref DS18X20::poll() не ждёт в цикле. Каждый вызов проверяет TIM1 UIF:
 * если операция ещё идёт — выход; если завершилась — один шаг FSM и запуск
 * следующей аппаратной операции. CPU свободен на время reset (~960 мкс),
 * Convert (~1 мс передачи + 750 мс ожидания) и чтения scratchpad (~4.5 мс).
 *
 * @par Связь с приложением
 * Драйвер не знает про UART и очередь событий. Результат уходит в колбэк
 * @ref DS18X20::SampleFn (слот + температура в десятых °C либо ErrorStatus).
 *
 * @see ds18x20_scan.hpp  блокирующий Search ROM только на старте
 * @see fsm_table_visual.md
 */

#pragma once

#include "ds18x20_hw.hpp"

/**
 * @class DS18X20
 * @brief Конечный автомат измерения температуры на общей 1-Wire шине.
 *
 * Типичное использование:
 * @code
 * static DS18X20 sensor;
 * sensor.init();
 * sensor.set_sample_callback(on_sample);
 * // в суперлупе:
 * sensor.poll();
 * @endcode
 *
 * После @c ds18x20_scan_bus() обязательно вызвать @ref rearm() — скан
 * перехватывает PA8 и TIM1.
 */
class DS18X20 {
public:
    /**
     * @brief Коды ошибок вместо температуры (десятые °C).
     *
     * Значения лежат ниже рабочего диапазона датчика (−55.0…+125.0 °C),
     * поэтому их можно отличить сравнением @ref is_error().
     */
    enum ErrorStatus : int16_t {
        TEMP_ERROR_GENERIC = INT16_MIN, ///< Непредусмотренный сбой FSM.
        TEMP_ERROR_NO_SENSOR,           ///< Нет presence: шина пуста, все слоты.
        TEMP_ERROR_CRC_FAIL             ///< CRC scratchpad не совпал.
    };

    /**
     * @brief Колбэк готового слота.
     * @param slot  Индекс в @c DS18X20_SENSORS.
     * @param temp  Десятые доли °C либо значение из @ref ErrorStatus.
     */
    using SampleFn = void (*)(uint8_t slot, int16_t temp);

    /**
     * @brief Включает тактирование TIM1 / GPIOA / DMA1, настраивает PA8 и
     *        переводит автомат в Idle с «заводным» UIF для первого @ref poll().
     */
    void init();

    /**
     * @brief Один шаг FSM, если текущая операция TIM1 завершилась (UIF).
     * @note Вызывать из основного цикла как можно чаще. Без UIF функция
     *       сразу возвращается — busy-wait нет.
     */
    void poll();

    /**
     * @brief Вернуть PA8 в AF/TIM1 после bit-bang скана и заново завести цикл.
     * @note Сбрасывает @c m_last_temp[] в @c TEMP_ERROR_NO_SENSOR.
     */
    void rearm();

    /**
     * @brief Подписаться на результаты измерений (и ошибки).
     * @param cb  Указатель на функцию или @c nullptr, чтобы отключить уведомления.
     */
    void set_sample_callback(SampleFn cb) { m_on_sample = cb; }

    /**
     * @brief Последнее значение слота (десятые °C) или код ошибки.
     * @param slot  Индекс датчика.
     * @return @c TEMP_ERROR_NO_SENSOR, если @p slot вне таблицы.
     */
    int16_t temperature(uint8_t slot) const {
        return (slot < DS18X20_SENSOR_COUNT) ? m_last_temp[slot]
                                             : static_cast<int16_t>(TEMP_ERROR_NO_SENSOR);
    }

    /**
     * @brief Признак, что значение — не температура, а @ref ErrorStatus.
     */
    static constexpr bool is_error(int16_t value) {
        return value <= TEMP_ERROR_CRC_FAIL;
    }

    /**
     * @brief Число слотов в @c DS18X20_SENSORS (compile-time).
     */
    static constexpr uint8_t sensor_count() { return DS18X20_SENSOR_COUNT; }

private:
    /**
     * @brief Фазы измерительного цикла.
     *
     * Переход по UIF: завершилась предыдущая аппаратная операция → выполняем
     * действие состояния и запускаем следующую. См. fsm_table_visual.md.
     */
    enum class State : uint8_t {
        Idle,       ///< Пауза между циклами / старт: reset перед Convert T.
        Convert,    ///< Presence? Skip ROM + Convert T : все слоты NO_SENSOR.
        Wait,       ///< Запуск ожидания 750 мс (конвертация).
        SlotReset,  ///< Reset перед чтением текущего слота.
        SlotSelect, ///< Presence? Match/Skip + Read Scratchpad : все слоты NO_SENSOR.
        SlotRead,   ///< Захват 72 бит scratchpad через DMA.
        SlotDecode  ///< CRC + температура; при CRC — повтор слота или CRC_FAIL.
    };

    /**
     * @brief 1-Wire family code (первый байт ROM).
     *        Значения — официальные коды Dallas/Maxim.
     */
    enum class Family : uint8_t {
        DS18S20 = 0x10, ///< 9-битный термометр, уточнение COUNT_REMAIN.
        DS18B20 = 0x28, ///< 12-битный термометр (по умолчанию).
    };

    static constexpr uint16_t kSkipCmdBits = 16;   ///< Два байта команды Skip ROM.
    static constexpr uint16_t kMatchReadBits = 80; ///< 0x55 + ROM[8] + 0xBE.

    /**
     * @brief Разделяемый буфер DMA: presence-фронты либо длительности 72 бит.
     * @note Декодирование scratchpad идёт в локальный массив, чтобы не
     *       затирать @c m_pulse[] через aliasing union.
     */
    union {
        volatile uint16_t m_edge[2];   ///< CCR2: конец reset и presence, мкс.
        volatile uint8_t m_pulse[72];  ///< Длительности слотов чтения, мкс.
    };
    State m_state = State::Idle;       ///< Текущая фаза FSM.
    Family m_family = Family::DS18B20; ///< Формула температуры текущего слота.
    uint8_t m_slot = 0;                ///< Индекс читаемого датчика.
    uint8_t m_attempts = 0;            ///< Уже выполненные чтения слота (CRC retry).
    int16_t m_last_temp[DS18X20_SENSOR_COUNT]{}; ///< Кэш последнего результата.
    SampleFn m_on_sample = nullptr;    ///< Необязательный получатель выборки.

    /**
     * @brief ROM слота задан (не восемь нулей).
     */
    bool rom_specified(uint8_t slot) const;

    /**
     * @brief Записать кэш и вызвать колбэк (если задан).
     */
    void emit(int16_t temp);

    /**
     * @brief Собрать scratchpad, проверить CRC; при сбое — повтор Match+Read.
     */
    void decode_and_report();

    /**
     * @brief Presence пропал: @c NO_SENSOR во все слоты и пауза.
     */
    void emit_bus_absent();

    /**
     * @brief Match ROM + Read или Skip ROM + Read для текущего слота.
     */
    void send_slot_read();

    /**
     * @brief Перевести сырой scratchpad в десятые доли °C с учётом @c m_family.
     * @return @c TEMP_ERROR_GENERIC, если у DS18S20 @c COUNT_PER_C == 0.
     */
    int16_t decode_temperature(const uint8_t pad[9]) const;

    /**
     * @brief Presence по двум захваченным фронтам reset-слота.
     */
    bool check_presence() const;

    /**
     * @brief PA8 = TIM1_CH1, AF2, open-drain, без внутренних подтяжек.
     */
    void configure_measurement_pin();

    /**
     * @brief Событие обновления TIM1: загрузить shadow-регистры, сбросить UIF.
     */
    void force_update();

    /**
     * @brief Запустить TIM1 в OPM на @p rcr+1 периодов по @p arr микросекунд.
     */
    void start_timer(uint16_t arr, uint8_t rcr);

    /**
     * @brief Reset 1-Wire (~480 мкс low) и захват presence в @c m_edge[].
     */
    void reset_bus();

    /**
     * @brief Передать @p bit_count бит: длительности в @p cmd (мкс), DMA в CCR1.
     * @param cmd        Первый импульс в @c cmd[0], далее DMA с @c cmd[1].
     * @param bit_count  Число бит (RCR = bit_count − 1, CNDTR = bit_count).
     */
    void send_command(const uint8_t *cmd, uint16_t bit_count);

    /**
     * @brief Прочитать 9 байт scratchpad: 72 read-слота, длительности в @c m_pulse[].
     */
    void read_data();

    /**
     * @brief Пауза ~250 мс между циклами и возврат в Idle.
     */
    void pause_or_idle();

    /**
     * @brief Следующий слот (reset → SlotSelect) или @ref pause_or_idle().
     */
    void next_slot_or_idle();
};
