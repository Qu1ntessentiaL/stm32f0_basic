# Политика оптимизации. Перезаписывает дефолты CMake (-O3 в Release, -g без -Og в Debug).
# 32 КБ flash: и Release, и MinSizeRel — это -Os, не -O3.

set(STM32_FLAGS_DEBUG          "-Og -g3")
set(STM32_FLAGS_RELEASE        "-Os -g0 -DNDEBUG")
set(STM32_FLAGS_MINSIZEREL     "-Os -g0 -DNDEBUG")
set(STM32_FLAGS_RELWITHDEBINFO "-Os -g3 -DNDEBUG")

foreach (_lang C CXX ASM)
    set(CMAKE_${_lang}_FLAGS_DEBUG          "${STM32_FLAGS_DEBUG}"          CACHE STRING "${_lang} Debug flags" FORCE)
    set(CMAKE_${_lang}_FLAGS_RELEASE        "${STM32_FLAGS_RELEASE}"        CACHE STRING "${_lang} Release flags" FORCE)
    set(CMAKE_${_lang}_FLAGS_MINSIZEREL     "${STM32_FLAGS_MINSIZEREL}"     CACHE STRING "${_lang} MinSizeRel flags" FORCE)
    set(CMAKE_${_lang}_FLAGS_RELWITHDEBINFO "${STM32_FLAGS_RELWITHDEBINFO}" CACHE STRING "${_lang} RelWithDebInfo flags" FORCE)
endforeach ()
