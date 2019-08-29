#pragma once

static constexpr bool TEST_SUCCESS {true};
static constexpr bool TEST_FAILED  {false};

#include <cstdio>
#define TEST_SUITE_START   \
    int main(int, char **) \
    {                      \
        bool ok {true};

#define TEST_SUITE_END      \
        return ok ? 0 : -1; \
    }

#define TEST(name, ...)                  \
    auto test_##name = []() __VA_ARGS__; \
    ok &= test_##name();                  \
    printf("[%s] " #name "\n", ok ? "  OK  " : "FAILED"); \
    ok = true;

#define ASSERT(x)                             \
    if (not (x)) {                            \
        printf("Assertion " #x " failed.\n"); \
        return TEST_FAILED;                   \
    }

#define TRACE(fmt, ...) \
    printf("    " fmt "\n", ##__VA_ARGS__);

