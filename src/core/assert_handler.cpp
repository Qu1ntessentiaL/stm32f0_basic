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
    // Signature: __glibcxx_assert_fail(const char*, int, const char*, const char*)
    void __glibcxx_assert_fail(const char *expr, int line, const char *file, const char *func) {
        (void)expr;
        (void)line;
        (void)file;
        (void)func;
        abort();
    }
}
