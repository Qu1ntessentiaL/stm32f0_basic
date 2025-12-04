#pragma once

struct App;

/**
 * Главный цикл приложения.
 * Вызывается в while(true) из main.cpp.
 */
void app_loop(App &app);