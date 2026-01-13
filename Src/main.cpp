#include "stm32f0xx.h"

#include "AppContext.hpp"
#include "app_loop.hpp"
#include "hardware_init.hpp"
#include "services_init.hpp"

App app{};

int main() {
    __disable_irq();

    hardware_init(app);   // Настройка всех драйверов и периферии

    __enable_irq();          // Включаем прерывания перед services_init (нужны для UART TX)

    services_init(app);   // Инициализация сервисов более высокого уровня

    while (true) {
        app_loop(app);
    }
}