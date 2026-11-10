#include "stdafx.h"
#include <gtest/gtest.h>
#include "Input.h"
#include <limits>

TEST(InputQueue, EmptyAndZeroSentinel)
{
    input::KeyQueue queue;
    EXPECT_EQ(0, queue.Pop());
    EXPECT_FALSE(queue.Push(0));
    EXPECT_EQ(0, queue.Pop());
}
TEST(InputQueue, FullDropsNewestAndPreservesOrder)
{
    input::KeyQueue queue;
    for (uint16_t i = 1; i <= 32; ++i) ASSERT_TRUE(queue.Push(i));
    EXPECT_FALSE(queue.Push(999));
    for (uint16_t i = 1; i <= 32; ++i) EXPECT_EQ(i, queue.Pop());
    EXPECT_EQ(0, queue.Pop());
}
TEST(InputQueue, InterleavedRepeatedWraps)
{
    input::KeyQueue queue;
    for (uint16_t i = 1; i <= 16; ++i) queue.Push(i);
    for (uint16_t i = 1; i < 1000; ++i)
    {
        EXPECT_EQ(i, queue.Pop());
        ASSERT_TRUE(queue.Push(i + 16));
    }
}
TEST(InputQueue, PreservesPressReleaseBitsAndIndependentInstances)
{
    input::KeyQueue first, second;
    first.Push(0x8072);
    first.Push(0072);
    EXPECT_EQ(0, second.Pop());
    EXPECT_EQ(0x8072, first.Pop());
    EXPECT_EQ(0072, first.Pop());
}
TEST(InputMapping, AlphabetInBothLayouts)
{
    constexpr uint8_t latin[] = {0072,0076,0050,0057,0033,0047,0055,0156,0073,0027,0052,0056,0112,
        0054,0075,0053,0067,0074,0111,0114,0051,0137,0071,0115,0070,0157};
    constexpr uint8_t russian[] = {0047,0073,0111,0071,0051,0072,0053,0074,0036,0075,0056,0057,0115,
        0114,0037,0157,0027,0052,0070,0033,0055,0112,0050,0110,0054,0067};
    for (int i = 0; i < 26; ++i)
    {
        EXPECT_EQ(latin[i], input::MapKey('A' + i, true)) << i;
        EXPECT_EQ(russian[i], input::MapKey('A' + i, false)) << i;
    }
}
TEST(InputMapping, DigitsAndKeypad)
{
    constexpr uint8_t digits[] = {0176,0030,0031,0032,0013,0034,0035,0016,0017,0177};
    constexpr uint8_t keypad[] = {0126,0127,0147,0167,0130,0150,0170,0125,0145,0165};
    for (bool latin : {false, true})
        for (int i = 0; i < 10; ++i)
        {
            EXPECT_EQ(digits[i], input::MapKey('0' + i, latin));
            EXPECT_EQ(keypad[i], input::MapKey(0x60 + i, latin));
        }
}
TEST(InputMapping, ControlsAndLayoutSpecificPunctuation)
{
    for (bool latin : {false, true})
    {
        EXPECT_EQ(0153, input::MapKey(13, latin));
        EXPECT_EQ(0166, input::MapKey(14, latin));
        EXPECT_EQ(0105, input::MapKey(16, latin));
        EXPECT_EQ(0113, input::MapKey(32, latin));
        EXPECT_EQ(0, input::MapKey(255, latin));
    }
    EXPECT_EQ(0174, input::MapKey(0xba, true));
    EXPECT_EQ(0137, input::MapKey(0xba, false));
    EXPECT_EQ(0036, input::MapKey(0xdb, true));
    EXPECT_EQ(0156, input::MapKey(0xdb, false));
}
TEST(InputTransition, EveryKeyReleasesOriginalLayoutAndSuppressesRepeat)
{
    for (bool latin : {false, true})
        for (int key = 0; key < 256; ++key)
        {
            uint8_t state = 0;
            const auto scan = input::MapKey(static_cast<uint8_t>(key), latin);
            EXPECT_EQ(scan ? (0x8000 | scan) : 0,
                input::KeyTransition(static_cast<uint8_t>(key), 128, state, latin));
            EXPECT_EQ(0, input::KeyTransition(static_cast<uint8_t>(key), 129, state, !latin));
            EXPECT_EQ(scan, input::KeyTransition(static_cast<uint8_t>(key), 0, state, !latin));
            EXPECT_EQ(0, state);
        }
}
TEST(InputTransition, ToggleBitsDoNotGenerateEvents)
{
    uint8_t state = 0;
    EXPECT_EQ(0, input::KeyTransition('A', 1, state, true));
    EXPECT_EQ(0, state);
}
TEST(InputTransition, NextPressUsesNewLayout)
{
    uint8_t state = 0;
    EXPECT_EQ(0x803a, input::KeyTransition('A', 128, state, true));
    EXPECT_EQ(0072, input::KeyTransition('A', 0, state, false));
    EXPECT_EQ(0x8027, input::KeyTransition('A', 128, state, false));
}
TEST(InputEnter, MainKeyOnly)
{
    std::array<uint8_t, 256> keys{};
    bool enter = true, numpad = false, previous = false;
    keys[13] = 128;
    input::NormalizeEnter(keys, enter, numpad, previous);
    EXPECT_EQ(128, keys[13]);
    EXPECT_EQ(0, keys[14]);
    EXPECT_TRUE(previous);
}
TEST(InputEnter, NumpadOnlyAndBothKeys)
{
    for (bool main : {false, true})
    {
        std::array<uint8_t, 256> keys{};
        bool enter = main, numpad = true, previous = false;
        keys[13] = 128;
        input::NormalizeEnter(keys, enter, numpad, previous);
        EXPECT_EQ(main ? 128 : 0, keys[13]);
        EXPECT_EQ(128, keys[14]);
        EXPECT_TRUE(previous);
    }
}
TEST(InputEnter, ReleaseAndIdleClearTracking)
{
    std::array<uint8_t, 256> keys{};
    bool enter = false, numpad = false, previous = true;
    input::NormalizeEnter(keys, enter, numpad, previous);
    EXPECT_EQ(0, keys[14]);
    EXPECT_FALSE(previous);
    enter = numpad = true;
    input::NormalizeEnter(keys, enter, numpad, previous);
    EXPECT_FALSE(enter);
    EXPECT_FALSE(numpad);
}
TEST(InputEnter, OneFrameReleaseWithRemainingKey)
{
    for (bool main : {false, true})
    {
        std::array<uint8_t, 256> keys{};
        bool enter = main, numpad = !main, previous = true;
        input::NormalizeEnter(keys, enter, numpad, previous);
        EXPECT_EQ(128, keys[14]);
        EXPECT_FALSE(previous);
    }
}
TEST(InputHitTest, InclusiveTopLeftExclusiveBottomRightAndOffsets)
{
    constexpr uint16_t keys[] = {10, 20, 5, 6, 0072};
    for (int x = 8; x <= 16; ++x)
        for (int y = 18; y <= 27; ++y)
            EXPECT_EQ(x >= 10 && x < 15 && y >= 20 && y < 26 ? 0072 : 0,
                input::HitTest(keys, x - 100, y + 200, -100, 200));
}
TEST(InputHitTest, EmptyIncompleteAndZeroSizeRectangles)
{
    EXPECT_EQ(0, input::HitTest({}, 0, 0, 0, 0));
    constexpr uint16_t keys[] = {0,0,0,5,1, 0,0,5,0,2, 0,0,5,5};
    EXPECT_EQ(0, input::HitTest(keys, 0, 0, 0, 0));
}
TEST(InputHitTest, OverlapUsesFirstMatchAndAdjacentUsesSecond)
{
    constexpr uint16_t keys[] = {0,0,5,5,1, 3,0,5,5,2};
    EXPECT_EQ(1, input::HitTest(keys, 3, 1, 0, 0));
    EXPECT_EQ(2, input::HitTest(keys, 5, 1, 0, 0));
}
TEST(InputHitTest, ExtremeOriginsDoNotWrap)
{
    constexpr uint16_t keys[] = {1,0,5,5,1};
    EXPECT_EQ(0, input::HitTest(keys, INT_MIN, 0, INT_MAX, 0));
}
TEST(InputMouse, NativeAndScaledMovementTruncateTowardZero)
{
    EXPECT_EQ(10, input::MouseAxis(110,100,0,640,640,640));
    EXPECT_EQ(5, input::MouseAxis(111,100,0,1280,1280,640));
    EXPECT_EQ(-5, input::MouseAxis(100,111,0,1280,1280,640));
    EXPECT_EQ(22, input::MouseAxis(111,100,0,320,320,640));
    EXPECT_EQ(5, input::MouseAxis(111,100,0,576,576,288));
}
TEST(InputMouse, OutsideAccelerationAndStationaryCursor)
{
    EXPECT_EQ(-15, input::MouseAxis(-10,0,0,640,640,640));
    EXPECT_EQ(15, input::MouseAxis(650,640,0,640,640,640));
    EXPECT_EQ(0, input::MouseAxis(-10,-10,0,640,640,640));
    EXPECT_EQ(0, input::MouseAxis(650,650,0,640,640,640));
    EXPECT_EQ(1, input::MouseAxis(640,639,0,640,640,640));
}
TEST(InputMouse, SaturationAndLargeCoordinatesDoNotWrap)
{
    EXPECT_EQ(127, input::MouseAxis(40000,0,0,640,640,640));
    EXPECT_EQ(-127, input::MouseAxis(0,40000,0,640,640,640));
    EXPECT_EQ(127, input::MouseAxis(INT_MAX,INT_MIN,0,640,640,640));
    EXPECT_EQ(-127, input::MouseAxis(INT_MIN,INT_MAX,0,640,640,640));
}
TEST(InputMouse, InvalidDimensionsAreInert)
{
    EXPECT_EQ(0, input::MouseAxis(10,0,0,0,0,640));
    EXPECT_EQ(0, input::MouseAxis(10,0,0,0,-1,640));
    EXPECT_EQ(0, input::MouseAxis(10,0,0,640,640,0));
}
TEST(InputMouse, ButtonsRequireFocusAndDownBit)
{
    for (int state = 0; state <= 65535; ++state)
    {
        EXPECT_EQ((state & 0x8000) != 0, input::MouseButton(static_cast<int16_t>(state), true));
        EXPECT_FALSE(input::MouseButton(static_cast<int16_t>(state), false));
    }
}
