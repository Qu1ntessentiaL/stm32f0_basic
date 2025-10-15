// chlib_stub.c — заглушка для ChibiOS/NIL
#include "ch.h"

#if defined(_CHIBIOS_NIL_CONF_) && !defined(__CHLIB_STUB_DEFINED__)
#define __CHLIB_STUB_DEFINED__

void __core_init(void) {}

void __heap_init(void) {}

void __factory_init(void) {}

#endif
