#pragma once

static constexpr bool TEST_SUCCESS {true};
static constexpr bool TEST_FAILED  {false};

#include <cstdio>
#define TEST_SUITE_START   \
    int main(int, char **) \
    {                      \
        bool ok {false};

#define TEST_SUITE_END     \
    }

#define TEST(name, ...)                  \
    auto test_##name = []() __VA_ARGS__; \
    printf("[ EXEC ] " #name "\n"); \
    ok = test_##name();                  \
    printf("[%s] " #name "\n", ok ? "  OK  " : "FAILED");

#define ASSERT(x)                             \
    if (not (x)) {                            \
        printf("Assertion " #x " failed.\n"); \
        return TEST_FAILED;                   \
    }

#define TRACE(fmt, ...) \
    printf("    " fmt "\n", ##__VA_ARGS__);

