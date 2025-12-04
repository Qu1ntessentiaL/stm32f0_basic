#pragma once

struct App;
struct Event;

/**
 *   Обработчик событий приложения.
 *   Все события из EventQueue попадают сюда.
 */
void dispatch_event(App& app, const Event& e);