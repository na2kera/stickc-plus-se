#pragma once
#include <driver/rmt.h>
#include "sirc.hpp"
namespace mini {
// Onboard IR LED is active-low on GPIO9. The driver stays installed after end();
// idle level holds the pin high so the LED is off between visits.
class IrTx {
  static constexpr rmt_channel_t channel_ = RMT_CHANNEL_0;
  static constexpr int irPin_ = 9, ledPin_ = 10;
  static constexpr int itemCapacity_ = sircRepeats * (sircPulses + 2);
  rmt_item32_t items_[itemCapacity_]{};
  bool installed_ = false, active_ = false, sending_ = false, failed_ = false;
public:
  void begin();
  void end();
  void poll();
  bool busy() const { return sending_; }
  bool active() const { return active_; }
  void send(uint8_t command, uint8_t device);
};
}
