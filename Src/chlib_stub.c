// chlib_stub.c — заглушка для ChibiOS/NIL
#include "ch.h"

__attribute__((section(".bss")))
stkalign_t main_thread_stack[128 / sizeof(stkalign_t)];

stkalign_t __main_thread_stack_base__ = (stkalign_t) & main_thread_stack[0];
stkalign_t __main_thread_stack_end__ = (stkalign_t) & main_thread_stack[sizeof(main_thread_stack) /
                                                                        sizeof(main_thread_stack[0])];

#if defined(_CHIBIOS_NIL_CONF_) && !defined(__CHLIB_STUB_DEFINED__)
#define __CHLIB_STUB_DEFINED__

void __core_init(void) {}

void __heap_init(void) {}

void __factory_init(void) {}

#endif
