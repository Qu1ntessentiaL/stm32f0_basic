// chlib_stub.c — заглушка для ChibiOS/NIL
#include "ch.h"

/* Стек главного потока (в .bss) */
__attribute__((section(".bss")))
stkalign_t main_thread_stack[128 / sizeof(stkalign_t)];

/* Эти символы объявлены у тебя как `extern stkalign_t ...` */
stkalign_t __main_thread_stack_base__;
stkalign_t __main_thread_stack_end__;

/* Функция инициализации — вызови её ДО chSysInit() */
void chlib_init_main_stack(void) {
    /* Безопасный cast через uintptr_t на случай, если stkalign_t — целочисленный тип */
    __main_thread_stack_base__ = (stkalign_t) (uintptr_t) &main_thread_stack[0];
    __main_thread_stack_end__ = (stkalign_t) (uintptr_t) &main_thread_stack[
            sizeof(main_thread_stack) / sizeof(main_thread_stack[0])];
}

#if defined(_CHIBIOS_NIL_CONF_) && !defined(__CHLIB_STUB_DEFINED__)
#define __CHLIB_STUB_DEFINED__

void __core_init(void) {}

void __heap_init(void) {}

void __factory_init(void) {}

#endif
