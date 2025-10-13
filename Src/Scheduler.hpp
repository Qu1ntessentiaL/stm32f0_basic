#pragma once

#include "stm32f0xx.h"
#include <array>
#include <functional>
#include <cstdint>

extern "C" void SysTick_Handler(void);

class TaskScheduler {
public:
    using TaskFunc = std::function<void(void)>;

    struct Task {
        TaskFunc func;
        uint32_t period_ms;
        uint32_t last_run;
    };

    static constexpr size_t MaxTasks = 8;

    void init() {
        SystemCoreClockUpdate();
        SysTick_Config(SystemCoreClock / 1000);
    }

    void addTask(TaskFunc func, uint32_t period_ms) {
        if (task_count < MaxTasks) {
            tasks[task_count++] = {func, period_ms, 0};
        }
    }

    void run() {
        while (true) {
            uint32_t now = millis();
            for (size_t i = 0; i < task_count; i++) {
                if ((now - tasks[i].last_run) >= tasks[i].period_ms) {
                    tasks[i].last_run = now;
                    tasks[i].func();
                }
            }
        }
    }

    static uint32_t millis() { return ms_ticks; }

private:
    std::array<Task, MaxTasks> tasks{};
    size_t task_count = 0;
    static inline volatile uint32_t ms_ticks = 0;

    friend void SysTick_Handler(void);
};