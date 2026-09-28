#pragma once

#include <cstdio>
#include <cstdlib>

inline void DeveloperStudioTestCheck(bool passed, const char* expression,
                                     const char* file, int line) {
    if (passed) return;
    std::fprintf(stderr, "%s:%d: check failed: %s\n", file, line, expression);
    std::abort();
}

#define TEST_CHECK(expression) \
    do { \
        const bool developerStudioTestCheckResult = static_cast<bool>(expression); \
        DeveloperStudioTestCheck(developerStudioTestCheckResult, #expression, __FILE__, __LINE__); \
    } while (false)
