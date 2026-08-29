#pragma once

#include <cstdio>
#include <cstdlib>
#include <string_view>

namespace mathhunter::test {

inline int g_failures = 0;

}  // namespace mathhunter::test

#define MH_CHECK(cond)                                                                 \
    do {                                                                               \
        if (!(cond)) {                                                                 \
            std::fprintf(stderr, "[FAIL] %s:%d: CHECK(%s)\n", __FILE__, __LINE__, #cond); \
            ++::mathhunter::test::g_failures;                                          \
        }                                                                              \
    } while (0)

#define MH_REQUIRE(cond)                                                                \
    do {                                                                                \
        if (!(cond)) {                                                                  \
            std::fprintf(stderr, "[FAIL] %s:%d: REQUIRE(%s)\n", __FILE__, __LINE__, #cond); \
            ++::mathhunter::test::g_failures;                                           \
            return ::mathhunter::test::g_failures;                                      \
        }                                                                               \
    } while (0)

#define MH_TEST_MAIN_EPILOGUE()                                                         \
    do {                                                                                \
        if (::mathhunter::test::g_failures == 0) {                                      \
            std::fprintf(stderr, "[OK] %s: all checks passed\n", __FILE__);             \
        } else {                                                                        \
            std::fprintf(stderr, "[FAIL] %s: %d check(s) failed\n", __FILE__,           \
                          ::mathhunter::test::g_failures);                              \
        }                                                                               \
        return ::mathhunter::test::g_failures == 0 ? 0 : 1;                             \
    } while (0)
