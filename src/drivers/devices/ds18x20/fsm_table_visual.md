# Драйвер DS18X20

Неблокирующий драйвер Dallas/Maxim **DS18B20** и **DS18S20** на STM32F030
(PA8 / TIM1_CH1 / DMA1). Исходники: `ds18x20.hpp`, `ds18x20.cpp`.
Инвентаризация шины вынесена в `ds18x20_scan.*` и на рабочий цикл не влияет.

## Идея адресации

На одной 1-Wire шине несколько датчиков. У каждого свой 64-битный ROM
(`family + serial + CRC`). Номер датчика в прошивке — **индекс** в таблице
`DS18X20_SENSORS` (`config.h`), а не «кто ответил первым».

| Операция | Команда | Зачем |
|---|---|---|
| Convert T | Skip ROM `0xCC 0x44` | Все чипы считают температуру параллельно; готовность по DQ, потолок 750 мс |
| Read Scratchpad | Match ROM `0x55` + ROM[8] + `0xBE` | Читаем только выбранный слот |
| Read Scratchpad | Skip ROM `0xCC 0xBE` | Только если слот один и ROM нулевой (стенд) |

При двух и более слотах нулевой ROM запрещён (`static_assert`): Skip ROM
на чтении смешает ответы.

Результат слота — колбэк `SampleFn(slot, temp)`: десятые °C либо `ErrorStatus`
(`NO_SENSOR`, `CRC_FAIL`, `GENERIC`). Драйвер не трогает UART и очередь событий.

## Железо

- **PA8** — 1-Wire, AF2 TIM1_CH1, **open-drain**, внешняя подтяжка.
- **TIM1** — 1 МГц (1 тик = 1 мкс), one-pulse mode. UIF = «операция закончилась».
- **DMA1_CH3** — захват presence (`m_edge[2]`) или 72 длительности read-слотов (`m_pulse[72]`).
- **DMA1_CH4** — подгрузка длительностей записи в `TIM1->CCR1`.

`poll()` смотрит только `TIM1->SR.UIF`. Нет флага — сразу return. Busy-wait
в измерительном контуре нет.

Команды (Skip Convert, Skip Read, Match+Read на каждый слот) разворачиваются
в импульсы **на этапе компиляции** и лежат во flash.

## FSM измерения

Линейный `switch` в `DS18X20::poll()`. Переход срабатывает, когда досчитался
предыдущий таймер/DMA. `m_slot` — какой датчик читаем в этом проходе.

```
                    ┌──────────────────────────────────────────┐
                    │                                          │
                    ▼                                          │
              ┌─────────┐   reset (~960 мкс)                   │
   UIF/prime ─►  Idle   ──────────────────────────────────► Convert
              └─────────┘                                      │
                    ▲                                          │
                    │ pause ~250 мс                            ▼
                    │                                 presence?
                    │                              нет / да
                    │                               │     │
                    │                               │     │ Skip ROM + Convert T
                    │                               │     ▼
                    │                               │   Wait ──► 62.5 мс
                    │                               │     │
                    │                               │     ▼
                    │                               │  WaitPoll ──► read-слот DQ
                    │                               │     │
                    │                               │     ▼
                    │                               │  WaitCheck: DQ=1 или 750 мс?
                    │                               │     нет: снова 62.5 мс
                    │                               │     да: reset ──► SlotSelect
                    │                               │     │
                    │                               │  presence?
                    │                               │  нет: NO_SENSOR всем слотам, pause → Idle
                    │                               │  да: Match/Skip + Read cmd
                    │                               │     ▼
                    │                               │  SlotRead ──► 72 бита DMA
                    │                               │     ▼
                    │                               │  SlotDecode
                    │                               │     │
                    │                               │  CRC fail и attempt < SENSOR_MAX_RETRIES?
                    │                               │  да: reset → SlotSelect (тот же слот)
                    │                               │     │
                    │                               │  есть ещё слот?
                    │                               │  да: ++m_slot, reset → SlotSelect ─┐
                    │                               │  нет: pause ~250 мс → Idle          │
                    │                               │                                      │
                    └───────────────────────────────┴──────────────────────────────────────┘
```

### Таблица состояний

| Состояние | Что уже завершилось (UIF) | Действие | Следующее состояние |
|---|---|---|---|
| **Idle** | Пауза между циклами или UG после `rearm()` | `m_slot = 0`, `reset_bus()` | Convert |
| **Convert** | Reset перед Convert T | Нет presence → `NO_SENSOR` всем слотам, пауза. Есть → Skip ROM + `0x44` | Idle / Wait |
| **Wait** | Передача Convert T | Пауза 62.5 мс | WaitPoll |
| **WaitPoll** | Квант ожидания | Один read-слот готовности (DQ) | WaitCheck |
| **WaitCheck** | Read-слот DQ | 1 или 12×62.5 мс → `reset_bus()`. Иначе ещё пауза | SlotSelect / WaitPoll |
| **SlotSelect** | Reset перед чтением | Нет presence → `NO_SENSOR` всем слотам. Есть → Match/Skip + `0xBE` | Idle / SlotRead |
| **SlotRead** | Передача команды чтения | `read_data()` — 72 слота в `m_pulse[]` | SlotDecode |
| **SlotDecode** | Захват scratchpad | CRC ок → температура и следующий слот. CRC fail → повтор Match+Read до `SENSOR_MAX_RETRIES`, иначе `CRC_FAIL` | SlotSelect или Idle |

### Ошибки

| Условие | Код | Что дальше |
|---|---|---|
| Нет presence на Convert или SlotSelect | `TEMP_ERROR_NO_SENSOR` на **каждый** слот | Пауза → Idle (шина общая, дальше читать бессмысленно) |
| CRC scratchpad не сходится | повтор Match+Read до `SENSOR_MAX_RETRIES`, затем `TEMP_ERROR_CRC_FAIL` | Новый Convert T не запускается |
| DS18S20 и `COUNT_PER_C == 0` | `TEMP_ERROR_GENERIC` | Слот засчитан, идём к следующему |
| `temperature(slot)` при slot ≥ N | `TEMP_ERROR_NO_SENSOR` | Только геттер, FSM не меняется |

Приложение (`services_init`) пушит и ошибки в `TemperatureReady`. Контроллер
по `DS18X20::is_error()` на слоте `DS18X20_CONTROL_SLOT` переходит в Error
и гасит нагрев.

### Семейство датчика

- ROM задан → `Family` из `rom[0]` (`0x28` DS18B20, `0x10` DS18S20).
- ROM нулевой (один датчик) → эвристика `scratchpad[4] == 0xFF` ⇒ DS18S20
  (нет config-регистра). Формула температуры разная: B20 сдвигает 12 бит,
  S20 использует COUNT_REMAIN / COUNT_PER_C.

## Search ROM (отдельный модуль)

`ds18x20_scan_bus()` — Maxim AN187, GPIO bit-bang, блокирующие `delay_us()`.
Только старт, флаг `DS18X20_BUS_SCAN_ENABLED`. Печатает ROM в виде
`{{0x28, ...}}` для вставки в `DS18X20_SENSORS`. После скана обязателен
`DS18X20::rearm()`: вернуть AF на PA8 и завести UIF для первого `poll()`.

Порядок Search ROM — порядок битового дерева, **не** слоты приложения.
Слоты назначаются вручную по напечатанным серийникам.

## Типичный цикл по времени

```
Idle  reset 960 мкс
Convert  команда ~1 мс
Wait/Poll DQ  до 750 мс (часто раньше)
для каждого слота:
    reset 960 мкс + Match/Read ~5 мс + decode (CPU, единицы мкс)
pause 250 мс
→ снова Idle
```

При одном датчике период ≈ 1 с. CPU занят только в моменты `poll()` с установленным UIF.
