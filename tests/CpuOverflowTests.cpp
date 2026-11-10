#include <gtest/gtest.h>
#include <cstdint>
#include <array>
extern "C" int CpuAddOverflow8(uint8_t, uint8_t);
extern "C" int CpuAddOverflow16(uint16_t, uint16_t);
extern "C" int CpuSubOverflow8(uint8_t, uint8_t);
extern "C" int CpuSubOverflow16(uint16_t, uint16_t);
static testing::AssertionResult CheckOverflow(unsigned a,unsigned b,unsigned bits)
{
    const int limit=1<<(bits-1);
    const int x=a<static_cast<unsigned>(limit)?static_cast<int>(a):static_cast<int>(a)-2*limit;
    const int y=b<static_cast<unsigned>(limit)?static_cast<int>(b):static_cast<int>(b)-2*limit;
    const bool add=x+y < -limit || x+y>=limit,sub=x-y < -limit || x-y>=limit;
    const int actualAdd=bits==8?CpuAddOverflow8(a,b):CpuAddOverflow16(a,b);
    const int actualSub=bits==8?CpuSubOverflow8(a,b):CpuSubOverflow16(a,b);
    if(actualAdd==static_cast<int>(add) && actualSub==static_cast<int>(sub)) return testing::AssertionSuccess();
    return testing::AssertionFailure()<<bits<<" bits, a="<<a<<", b="<<b<<": add="<<actualAdd<<" expected "<<add<<", sub="<<actualSub<<" expected "<<sub;
}
TEST(CpuOverflowTests, EveryByteOperandPair)
{
    for(unsigned a=0;a<256;++a) for(unsigned b=0;b<256;++b) ASSERT_TRUE(CheckOverflow(a,b,8));
}
class WordOverflowEdges : public testing::TestWithParam<unsigned> {};
TEST_P(WordOverflowEdges, EveryWordOperandInBothOrders)
{
    for(unsigned b=0;b<65536;++b) { ASSERT_TRUE(CheckOverflow(GetParam(),b,16)); ASSERT_TRUE(CheckOverflow(b,GetParam(),16)); }
}
INSTANTIATE_TEST_SUITE_P(Boundaries,WordOverflowEdges,testing::Values(0u,1u,2u,32766u,32767u,32768u,32769u,65534u,65535u));
TEST(CpuOverflowTests, MillionDeterministicWordPairs)
{
    uint32_t state=1;
    for(unsigned i=0;i<1000000;++i) {
        state=state*1664525u+1013904223u; const unsigned a=state>>16;
        state=state*1664525u+1013904223u; ASSERT_TRUE(CheckOverflow(a,state>>16,16));
    }
}
