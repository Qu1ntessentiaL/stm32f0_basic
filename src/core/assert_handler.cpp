// Minimal assert handlers to avoid pulling in heavy stdio formatting routines
#include <cstdlib>

extern "C" void __assert_func(const char *file, int line, const char *func, const char *expr) {
    (void)file;
    (void)line;
    (void)func;
    (void)expr;
    // Minimal behaviour: halt (could call abort to trigger debugger)
    abort();
}

namespace std {
    // Сигнатура должна точно совпадать с объявлением в <bits/c++config.h>:
    //   extern "C++" _GLIBCXX_NORETURN __attribute__((__cold__))
    //   void __glibcxx_assert_fail(const char* file, int line,
    //                              const char* function, const char* condition) noexcept;
    // Без noexcept/[[noreturn]] компилятор считает это другой перегрузкой
    // с несовпадающим exception specifier -> предупреждение -Wpedantic.
    [[noreturn]] void __glibcxx_assert_fail(const char *file, int line,
                                             const char *function, const char *condition) noexcept {
        (void)file;
        (void)line;
        (void)function;
        (void)condition;
        abort();
    }
}
