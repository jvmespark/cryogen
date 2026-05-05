#pragma once
//
// tests/test_util.hpp
//

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>

#define CRYOGEN_CHECK(cond) do {                                                      \
    if (!(cond)) {                                                                    \
        std::fprintf(stderr, "[FAIL] %s:%d  %s\n", __FILE__, __LINE__, #cond);        \
        std::exit(1);                                                                 \
    }                                                                                 \
} while (0)

inline bool approx_eq(float a, float b, float eps = 1e-4f) {
    return std::fabs(a - b) <= eps;
}
