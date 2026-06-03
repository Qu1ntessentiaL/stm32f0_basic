# Реализация таблицы переходов для DS18B20 FSM

## Текущая реализация: Transition с указателями на функции-члены

### Структура записи таблицы:

```cpp
struct Transition {
    FsmStates state;                  ///< Исходное состояние
    bool (DS18B20::*guard)() const;   ///< Условие перехода (nullptr = безусловный)
    void (DS18B20::*action)();        ///< Действие при переходе
    FsmStates next;                   ///< Целевое состояние
};
```

### Визуализация таблицы переходов:

```
┌─────────────┬───────────────────────┬──────────────────────────┬─────────────┐
│ Текущее     │ Условие               │ Действие                 │ Следующее   │
│ состояние   │                       │                          │ состояние   │
├─────────────┼───────────────────────┼──────────────────────────┼─────────────┤
│ IDLE        │ всегда                │ action_idle()            │ START       │
│             │                       │ (инициализация +         │             │
│             │                       │  fallthrough)            │             │
│ START       │ всегда                │ action_start()           │ CONVERT     │
│             │                       │ (LED on + reset_bus)     │             │
│ CONVERT     │ check_presence_ok()   │ action_convert_ok()      │ WAIT        │
│             │                       │ (send convert command)   │             │
│ CONVERT     │ check_presence_fail() │ action_convert_fail()    │ IDLE        │
│             │                       │ (error + pause)          │             │
│ WAIT        │ всегда                │ action_wait()            │ CONTINUE    │
│             │                       │ (wait conversion)        │             │
│ CONTINUE    │ всегда                │ action_continue()        │ REQUEST     │
│             │                       │ (reset bus)              │             │
│ REQUEST     │ check_presence_ok()   │ action_request_ok()      │ READ        │
│             │                       │ (send read command)      │             │
│ REQUEST     │ check_presence_fail() │ action_request_fail()    │ IDLE        │
│             │                       │ (error + pause)          │             │
│ READ        │ всегда                │ action_read()            │ DECODE      │
│             │                       │ (read scratchpad)        │             │
│ DECODE      │ всегда                │ action_decode()          │ IDLE        │
│             │                       │ (decode + CRC check)     │             │
└─────────────┴───────────────────────┴──────────────────────────┴─────────────┘
```

### Реальная таблица переходов (из кода):

```cpp
const DS18B20::Transition DS18B20::m_transitions[] = {
    // IDLE -> START (безусловный, fallthrough - выполняем action_idle и сразу переходим в START)
    {FsmStates::IDLE,     nullptr,                &DS18B20::action_idle,     FsmStates::START},
    
    // START -> CONVERT (безусловный)
    {FsmStates::START,    nullptr,                &DS18B20::action_start,    FsmStates::CONVERT},
    
    // CONVERT -> WAIT (если присутствует)
    {FsmStates::CONVERT,  &DS18B20::check_presence_ok, &DS18B20::action_convert_ok, FsmStates::WAIT},
    
    // CONVERT -> IDLE (если отсутствует)
    {FsmStates::CONVERT,  &DS18B20::check_presence_fail, &DS18B20::action_convert_fail, FsmStates::IDLE},
    
    // WAIT -> CONTINUE (безусловный)
    {FsmStates::WAIT,     nullptr,                &DS18B20::action_wait,     FsmStates::CONTINUE},
    
    // CONTINUE -> REQUEST (безусловный)
    {FsmStates::CONTINUE, nullptr,                &DS18B20::action_continue, FsmStates::REQUEST},
    
    // REQUEST -> READ (если присутствует)
    {FsmStates::REQUEST,  &DS18B20::check_presence_ok, &DS18B20::action_request_ok, FsmStates::READ},
    
    // REQUEST -> IDLE (если отсутствует)
    {FsmStates::REQUEST,  &DS18B20::check_presence_fail, &DS18B20::action_request_fail, FsmStates::IDLE},
    
    // READ -> DECODE (безусловный)
    {FsmStates::READ,     nullptr,                &DS18B20::action_read,     FsmStates::DECODE},
    
    // DECODE -> IDLE (безусловный, CRC проверяется внутри)
    {FsmStates::DECODE,   nullptr,                &DS18B20::action_decode,   FsmStates::IDLE},
};
```

### Реальная реализация poll():

```cpp
void DS18B20::poll() {
    // Check if timer update interrupt occurred (indicates operation completion)
    if (!(TIM1->SR & TIM_SR_UIF)) return;
    TIM1->SR = 0;

    // Поиск подходящего перехода в таблице
    bool transition_found = false;
    bool need_fallthrough = false;
    
    for (const auto &t : m_transitions) {
        if (t.state == m_ctx.current_state) {
            // Проверка условия (если есть)
            if (!t.guard || (this->*t.guard)()) {
                // Выполнение действия
                (this->*t.action)();
                // Переход в следующее состояние
                m_ctx.current_state = t.next;
                transition_found = true;
                
                // Обработка fallthrough: IDLE -> START выполняется сразу
                if (t.next == FsmStates::START) {
                    need_fallthrough = true;
                    // Продолжаем поиск для START
                    continue;
                }
                break;
            }
        }
    }
    
    // Если нужен fallthrough (IDLE -> START), выполняем START -> CONVERT сразу
    if (need_fallthrough && m_ctx.current_state == FsmStates::START) {
        for (const auto &t : m_transitions) {
            if (t.state == FsmStates::START) {
                if (!t.guard || (this->*t.guard)()) {
                    (this->*t.action)();
                    m_ctx.current_state = t.next;
                    break;
                }
            }
        }
    }

    // Если переход не найден - ошибка
    if (!transition_found) {
        ds18b20_temp_ready(ErrorStatus::TEMP_ERROR_GENERIC
#if defined ELAPSED_TIME
                , DWT->CYCCNT - elapsed_time
#endif
        );
        m_ctx.current_state = FsmStates::IDLE;
    }
}
```

### Методы-действия:

Все действия вынесены в отдельные методы для лучшей читаемости:

- `action_idle()` - инициализация цикла измерения (заполнение union, установка elapsed_time)
- `action_start()` - начало измерения (LED on, reset_bus)
- `action_convert_ok()` - отправка команды конвертации (если датчик присутствует)
- `action_convert_fail()` - обработка ошибки отсутствия датчика
- `action_wait()` - ожидание завершения конвертации
- `action_continue()` - подготовка к чтению (reset_bus)
- `action_request_ok()` - отправка команды чтения (если датчик присутствует)
- `action_request_fail()` - обработка ошибки отсутствия датчика
- `action_read()` - чтение данных из scratchpad
- `action_decode()` - декодирование данных, проверка CRC, отправка результата

### Методы-условия:

- `check_presence_ok()` - проверка наличия датчика (возвращает `check_presence()`)
- `check_presence_fail()` - проверка отсутствия датчика (возвращает `!check_presence()`)

### Преимущества реализации:

1. **Читаемость**: Все переходы видны в одном месте - в таблице `m_transitions[]`
2. **Масштабируемость**: Легко добавить новые состояния и переходы - просто добавить запись в таблицу
3. **Поддерживаемость**: Изменения вносятся только в таблицу и методы-действия
4. **Тестируемость**: Легко проверить все возможные переходы
5. **Документированность**: Таблица сама является документацией FSM
6. **Сохранение логики**: Полностью сохранена оригинальная логика, включая fallthrough IDLE->START

### Особенности реализации:

- **Fallthrough IDLE->START**: Сохранена оригинальная логика, когда IDLE и START выполняются в одном вызове `poll()`
- **Условные переходы**: CONVERT и REQUEST имеют два возможных перехода в зависимости от наличия датчика
- **CRC проверка**: Выполняется внутри `action_decode()`, не влияет на переход состояния
- **Обработка ошибок**: Неожиданные состояния обрабатываются и возвращают FSM в IDLE

### Гарантии сохранения логики:

✅ Все действия идентичны оригинальному switch-case коду  
✅ Порядок выполнения сохранен  
✅ Fallthrough IDLE->START работает так же  
✅ Условные переходы работают идентично  
✅ Обработка ошибок сохранена  
✅ Использование `elapsed_time` идентично (static на уровне файла)
