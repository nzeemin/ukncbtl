#pragma once
#include <array>
#include <cstdint>
#include <span>
namespace input
{
class KeyQueue
{
public:
    bool Push(uint16_t event);
    uint16_t Pop();
private:
    std::array<uint16_t, 32> events{};
    size_t head = 0, count = 0;
};
uint8_t MapKey(uint8_t key, bool latin);
uint16_t KeyTransition(uint8_t key, uint8_t current, uint8_t& previous, bool latin);
void NormalizeEnter(std::span<uint8_t, 256> keys, bool& enter, bool& numpad, bool& wasPressed);
uint8_t HitTest(std::span<const uint16_t> rectangles, int x, int y, int originX, int originY);
int16_t MouseAxis(int position, int previous, int low, int high, int displayedSize, int nativeSize);
bool MouseButton(int16_t state, bool focused);
}
