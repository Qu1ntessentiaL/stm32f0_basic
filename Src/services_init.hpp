#pragma once

struct App;

/**
 *   Инициализация сервисного уровня:
 *   - EventQueue
 *   - BeepManager
 *   - Controller
 *   - логирование (print_fw_info)
 */
void services_init(App &app);