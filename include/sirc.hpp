#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
namespace mini {
struct SircPulse { uint16_t markUs, spaceUs; }; // mark = carrier on
constexpr size_t sircPulses = 13;               // start + 12 bits
using SircFrame = std::array<SircPulse, sircPulses>;
constexpr uint32_t sircFramePeriodUs = 45000, sircRepeats = 3;
constexpr uint8_t sircTv = 1;
constexpr uint8_t sircPower = 21, sircChannelUp = 16, sircChannelDown = 17;
constexpr uint8_t sircVolumeUp = 18, sircVolumeDown = 19;
SircFrame sircEncode(uint8_t command, uint8_t device);
uint32_t sircFrameLengthUs(const SircFrame& frame);
}
