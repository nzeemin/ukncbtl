#include <gtest/gtest.h>
#include <cstdint>
#include <cstdio>

extern "C" int CpuAddOverflow8(uint8_t, uint8_t);
extern "C" int CpuAddOverflow16(uint16_t, uint16_t);
extern "C" int CpuSubOverflow8(uint8_t, uint8_t);
extern "C" int CpuSubOverflow16(uint16_t, uint16_t);

static bool check(unsigned a, unsigned b, unsigned bits)
{
    const int limit = 1 << (bits - 1);
    const int x = a < static_cast<unsigned>(limit) ? static_cast<int>(a) : static_cast<int>(a) - 2 * limit;
    const int y = b < static_cast<unsigned>(limit) ? static_cast<int>(b) : static_cast<int>(b) - 2 * limit;
    const bool add = x + y < -limit || x + y >= limit;
    const bool sub = x - y < -limit || x - y >= limit;
    const int actualAdd = bits == 8 ? CpuAddOverflow8(a, b) : CpuAddOverflow16(a, b);
    const int actualSub = bits == 8 ? CpuSubOverflow8(a, b) : CpuSubOverflow16(a, b);
    if (actualAdd != static_cast<int>(add) || actualSub != static_cast<int>(sub)) {
        std::printf("%u-bit mismatch: %u, %u: add=%d sub=%d\n", bits, a, b, actualAdd, actualSub);
        return false;
    }
    return true;
}

static int RunRegressionChecks()
{
    for (unsigned a = 0; a < 256; ++a)
        for (unsigned b = 0; b < 256; ++b)
            if (!check(a, b, 8)) return 1;
    const unsigned edges[] = {0, 1, 2, 32766, 32767, 32768, 32769, 65534, 65535};
    for (unsigned a : edges)
        for (unsigned b = 0; b < 65536; ++b)
            if (!check(a, b, 16) || !check(b, a, 16)) return 1;
    uint32_t state = 1;
    for (unsigned i = 0; i < 1000000; ++i) {
        state = state * 1664525u + 1013904223u;
        unsigned a = state >> 16;
        state = state * 1664525u + 1013904223u;
        if (!check(a, state >> 16, 16)) return 1;
    }
    return 0;
}

TEST(CpuOverflowTests, RegressionChecks)
{
    EXPECT_EQ(RunRegressionChecks(), 0);
}
