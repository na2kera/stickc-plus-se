#include "sirc.hpp"
namespace mini {
SircFrame sircEncode(uint8_t command, uint8_t device) {
  command &= 0x7f; device &= 0x1f; // 7 command bits, then 5 device bits.
  SircFrame frame{};
  frame[0] = {2400, 600};
  const uint32_t bits = command | (uint32_t(device) << 7); // LSB first.
  for (size_t i = 0; i < 12; ++i) {
    const uint16_t mark = (bits >> i) & 1u ? 1200 : 600;
    frame[i + 1] = {mark, 600};
  }
  return frame;
}
uint32_t sircFrameLengthUs(const SircFrame& frame) {
  uint32_t total = 0;
  for (const auto& pulse : frame) total += uint32_t(pulse.markUs) + pulse.spaceUs;
  return total;
}
}
