#include <etl/array.h>

#include "stm32f0xx.h"

#include "AppContext.hpp"
#include "app_loop.hpp"
#include "hardware_init.hpp"
#include "services_init.hpp"

App app{};

etl::array<int, 10> array1 = {
    0, 1, 2, 3, 4,
    5, 6, 7, 8, 9
};

#if defined ETL_NO_STL
    #define GOVNO 777
#endif


int main() {
    __disable_irq();

    hardware_init(app);   // Настройка всех драйверов и периферии
    services_init(app);   // Инициализация сервисов более высокого уровня

    __enable_irq();

    while (true) {
        app_loop(app);
    }
}