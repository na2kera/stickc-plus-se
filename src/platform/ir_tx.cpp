#include "ir_tx.hpp"
#include <Arduino.h>
#include <driver/gpio.h>
namespace mini {
namespace {
static_assert(sircRepeats * (sircPulses + 2) <= 63, "3 SIRC frames fit in one 64-word RMT block");
void appendIdle(rmt_item32_t* items, int& count, uint32_t us) {
  // Durations are 15-bit (max 32767 µs) and 0 ends the TX, so split the idle gap
  // into two level-1 items.
  const uint32_t q = us / 4;
  const uint32_t parts[4] = {q, q, q, us - 3 * q};
  for (int i = 0; i < 2; ++i) {
    auto& item = items[count++];
    item.duration0 = parts[i * 2]; item.level0 = 1;
    item.duration1 = parts[i * 2 + 1]; item.level1 = 1;
  }
}
}
void IrTx::begin() {
  if (installed_) { active_ = true; return; }
  if (failed_) return;
  // Latch HIGH before enabling output so neither active-low LED blinks during setup.
  digitalWrite(irPin_, HIGH); digitalWrite(ledPin_, HIGH);
  pinMode(irPin_, OUTPUT); pinMode(ledPin_, OUTPUT);
  rmt_config_t config{};
  config.rmt_mode = RMT_MODE_TX;
  config.channel = channel_;
  config.gpio_num = GPIO_NUM_9;
  config.clk_div = 80; // 80 MHz APB / 80 = 1 µs per tick.
  config.mem_block_num = 1;
  config.tx_config.carrier_freq_hz = 40000;
  config.tx_config.carrier_en = true;
  // Per the IDF enum, LOW means the carrier modulates level-0 (mark) items only; spaces and idle stay HIGH.
  config.tx_config.carrier_level = RMT_CARRIER_LEVEL_LOW;
  config.tx_config.idle_level = RMT_IDLE_LEVEL_HIGH;
  config.tx_config.idle_output_en = true;
  config.tx_config.loop_en = false;
  // Duty is HIGH time and the LED is on while low. 50% doubles the mean IR power
  // versus the 33% the bench test needed a few cm for; receivers accept it.
  config.tx_config.carrier_duty_percent = 50;
  if (rmt_config(&config) != ESP_OK || rmt_driver_install(channel_, 0, 0) != ESP_OK) {
    failed_ = true; digitalWrite(irPin_, HIGH); digitalWrite(ledPin_, HIGH);
    Serial.println("IR: setup failed"); return;
  }
  rmt_set_idle_level(channel_, true, RMT_IDLE_LEVEL_HIGH);
  gpio_set_drive_capability(GPIO_NUM_9, GPIO_DRIVE_CAP_3); // Strongest pad drive for the LED.
  installed_ = active_ = true;
  Serial.println("IR: RMT ch0 gpio9 40kHz SIRC");
}
void IrTx::end() {
  if (!active_) return;
  active_ = false;
  // Never rmt_tx_stop(): the driver's tx semaphore is only released by the TX_END
  // interrupt, so a stopped transfer could block the next rmt_write_items() forever.
  // A send finishes on its own within 135 ms; poll() clears sending_ on re-entry.
  digitalWrite(ledPin_, HIGH);
}
void IrTx::poll() {
  if (!sending_) return;
  if (rmt_wait_tx_done(channel_, 0) == ESP_OK) { sending_ = false; digitalWrite(ledPin_, HIGH); }
}
void IrTx::send(uint8_t command, uint8_t device) {
  if (!installed_) return;
  command &= 0x7f; device &= 0x1f;
  if (sending_) { Serial.println("IR: dropped"); return; }
  const auto frame = sircEncode(command, device);
  const uint32_t gap = sircFramePeriodUs - sircFrameLengthUs(frame);
  int count = 0;
  for (uint32_t n = 0; n < sircRepeats; ++n) {
    for (const auto& pulse : frame) {
      auto& item = items_[count++];
      item.duration0 = pulse.markUs; item.level0 = 0;
      item.duration1 = pulse.spaceUs; item.level1 = 1;
    }
    appendIdle(items_, count, gap);
  }
  if (rmt_write_items(channel_, items_, count, false) != ESP_OK) {
    Serial.println("IR: write failed"); return;
  }
  sending_ = true;
  digitalWrite(ledPin_, LOW);
  Serial.printf("IR: SIRC cmd=%u dev=%u\n", unsigned(command), unsigned(device));
}
}
