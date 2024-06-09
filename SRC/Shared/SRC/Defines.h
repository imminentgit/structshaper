#pragma once

#if defined(_MSC_VER)
    #define ALWAYS_INLINE __forceinline
#else
    #define ALWAYS_INLINE inline __attribute__((always_inline))
#endif

#define FORCE_CLASS_TO_BE_INSTANTIATED(T)                                                                              \
    namespace Hidden {                                                                                                 \
        [[maybe_unused]] int unused_##T = [] {                                                                         \
            [[maybe_unused]] auto unused_local_var##T = T::the();                                                      \
            return 1;                                                                                                  \
        }();                                                                                                           \
    }
