#pragma once
#include <cstdint>
#include "hid_report.hpp"
class BLECharacteristic;
class BLE2902;
namespace mini {
// BLE HID keyboard + consumer control. Uses the BLE library bundled with Arduino-ESP32 to build the
// services and the ESP-IDF GATTS API of the same framework to send and close; no extra library.
// Only SDK calls live here; the decisions are in HidRemote.
class BleKeyboard final : HidPort {
  BLECharacteristic* input_[2] = {};
  BLE2902* cccd_[2] = {};
  void start() override;
  void advertise(bool on) override;
  bool send(uint16_t connId, HidReport report, const uint8_t* data, size_t size) override;
  void store(HidReport report, const uint8_t* data, size_t size) override;
  void disconnect(uint16_t connId) override;
  void wait(uint32_t ms) override;
  uint32_t now() override;
public:
  HidRemote remote_; // Its link is updated from the Bluedroid (BT) task callbacks.
  static constexpr uint8_t rightArrow = 0x4F, leftArrow = 0x50;
  // Every loop, also when the screen does not want BLE: starts Bluedroid on first use (never
  // deinitialized), advertises, disconnects on leave and keeps recovering a stuck release.
  void poll(bool wanted) { remote_.poll(*this, wanted); }
  bool linked() const { return remote_.link.linked(); }  // GATT connected.
  void resetSubscription();  // BT task: forget both input report CCCDs on (dis)connect.
  bool ready() const { return remote_.link.ready(HidReport::Keyboard); }       // Host subscribed to report 1.
  bool volumeReady() const { return remote_.link.ready(HidReport::Consumer); } // Host subscribed to report 2.
  void tap(uint8_t keycode) { remote_.tap(*this, HidReport::Keyboard, keycode); } // Press then release; ignored unless ready().
  void tapVolumeUp() { remote_.tap(*this, HidReport::Consumer, hidVolumeUp); }    // Ignored unless volumeReady().
};
}
