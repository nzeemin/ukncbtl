#include "stdafx.h"
#include "Input.h"
#include <algorithm>
namespace input
{
bool KeyQueue::Push(uint16_t event)
{
    if (event == 0 || count == events.size()) return false;
    events[(head + count) % events.size()] = event;
    ++count;
    return true;
}
uint16_t KeyQueue::Pop()
{
    if (count == 0) return 0;
    auto event = events[head];
    head = (head + 1) % events.size();
    --count;
    return event;
}
const uint8_t arrPcscan2UkncscanLat[256] =    // LAT
{
    /*       0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f  */
    /*0*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0132, 0026, 0000, 0000, 0000, 0153, 0166, 0000,
    /*1*/    0105, 0000, 0000, 0000, 0107, 0000, 0000, 0000, 0000, 0000, 0000, 0006, 0000, 0000, 0000, 0000,
    /*2*/    0113, 0004, 0151, 0152, 0155, 0116, 0154, 0133, 0134, 0000, 0000, 0000, 0000, 0171, 0172, 0000,
    /*3*/    0176, 0030, 0031, 0032, 0013, 0034, 0035, 0016, 0017, 0177, 0000, 0000, 0000, 0000, 0000, 0000,
    /*4*/    0000, 0072, 0076, 0050, 0057, 0033, 0047, 0055, 0156, 0073, 0027, 0052, 0056, 0112, 0054, 0075,
    /*5*/    0053, 0067, 0074, 0111, 0114, 0051, 0137, 0071, 0115, 0070, 0157, 0000, 0000, 0106, 0000, 0000,
    /*6*/    0126, 0127, 0147, 0167, 0130, 0150, 0170, 0125, 0145, 0165, 0025, 0000, 0000, 0005, 0146, 0131,
    /*7*/    0010, 0011, 0012, 0014, 0015, 0172, 0152, 0151, 0171, 0000, 0004, 0155, 0000, 0000, 0000, 0000,
    /*8*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*9*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*a*/    0000, 0000, 0046, 0066, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*b*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0174, 0110, 0117, 0175, 0135, 0173,
    /*c*/    0007, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*d*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0036, 0136, 0037, 0077, 0000,
    /*e*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*f*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
};
const uint8_t arrPcscan2UkncscanRus[256] =    // RUS
{
    /*       0     1     2     3     4     5     6     7     8     9     a     b     c     d     e     f  */
    /*0*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0132, 0026, 0000, 0000, 0000, 0153, 0166, 0000,
    /*1*/    0105, 0000, 0000, 0000, 0107, 0000, 0000, 0000, 0000, 0000, 0000, 0006, 0000, 0000, 0000, 0000,
    /*2*/    0113, 0004, 0151, 0152, 0174, 0116, 0154, 0133, 0134, 0000, 0000, 0000, 0000, 0171, 0172, 0000,
    /*3*/    0176, 0030, 0031, 0032, 0013, 0034, 0035, 0016, 0017, 0177, 0000, 0000, 0000, 0000, 0000, 0000,
    /*4*/    0000, 0047, 0073, 0111, 0071, 0051, 0072, 0053, 0074, 0036, 0075, 0056, 0057, 0115, 0114, 0037,
    /*5*/    0157, 0027, 0052, 0070, 0033, 0055, 0112, 0050, 0110, 0054, 0067, 0000, 0000, 0106, 0000, 0000,
    /*6*/    0126, 0127, 0147, 0167, 0130, 0150, 0170, 0125, 0145, 0165, 0025, 0000, 0000, 0005, 0146, 0131,
    /*7*/    0010, 0011, 0012, 0014, 0015, 0172, 0152, 0151, 0171, 0000, 0004, 0174, 0000, 0000, 0000, 0000,
    /*8*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*9*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*a*/    0000, 0000, 0046, 0066, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*b*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0137, 0117, 0076, 0175, 0077, 0173,
    /*c*/    0007, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*d*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0156, 0135, 0155, 0136, 0000,
    /*e*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
    /*f*/    0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000, 0000,
};

void NormalizeEnter(std::span<uint8_t, 256> keys, bool& enter, bool& numpad, bool& wasPressed)
{
    if (keys[13] & 128)
    {
        if (enter && numpad)
            keys[13 + 1] = 128;
        if (!enter && numpad)
        {
            keys[13] = 0;
            keys[13 + 1] = 128;
        }
        wasPressed = true;
    }
    else
    {
        if (wasPressed)
        {
            if (enter) keys[13 + 1] = 128;
            if (numpad) keys[13 + 1] = 128;
        }
        else
        {
            enter = false;
            numpad = false;
        }
        wasPressed = false;
    }
}
uint8_t MapKey(uint8_t key, bool latin)
{
    return latin ? arrPcscan2UkncscanLat[key] : arrPcscan2UkncscanRus[key];
}
uint16_t KeyTransition(uint8_t key, uint8_t current, uint8_t& previous, bool latin)
{
    if ((current & 128) == (previous & 128)) return 0;
    // Release uses the layout saved at key-down.
    const auto scan = MapKey(key, (previous & 128) ? (previous & 2) != 0 : latin);
    previous = (current & 128) ? static_cast<uint8_t>(128 | (latin ? 2 : 0)) : 0;
    return scan == 0 ? 0 : static_cast<uint16_t>(scan | ((current & 128) << 8));
}
uint8_t HitTest(std::span<const uint16_t> rectangles, int x, int y, int originX, int originY)
{
    const int64_t localX = int64_t{x} - originX;
    const int64_t localY = int64_t{y} - originY;
    for (size_t i = 0; i + 4 < rectangles.size(); i += 5)
    {
        if (localX >= rectangles[i] && localX < int64_t{rectangles[i]} + rectangles[i + 2] &&
            localY >= rectangles[i + 1] && localY < int64_t{rectangles[i + 1]} + rectangles[i + 3])
            return static_cast<uint8_t>(rectangles[i + 4]);
    }
    return 0;
}
int16_t MouseAxis(int position, int previous, int low, int high, int displayedSize, int nativeSize)
{
    if (displayedSize <= 0 || nativeSize <= 0) return 0;
    int64_t delta = int64_t{position} - previous;
    if (delta != 0 && position < low) delta -= (int64_t{low} - position) / 2;
    if (delta != 0 && position > high) delta += (int64_t{position} - high) / 2;
    delta = delta * nativeSize / displayedSize;
    return static_cast<int16_t>(std::clamp<int64_t>(delta, -127, 127));
}
bool MouseButton(int16_t state, bool focused)
{
    return focused && (static_cast<uint16_t>(state) & 0x8000) != 0;
}
}