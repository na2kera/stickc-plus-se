#pragma once
#include <atomic>
#include <cstdint>
class BLEServer;
class BLECharacteristic;
class BLE2902;
namespace mini {
// BLE HID keyboard built only on the BLE library bundled with Arduino-ESP32.
class BleKeyboard {
  BLEServer* server_ = nullptr;
  BLECharacteristic* input_ = nullptr;
  BLE2902* inputCccd_ = nullptr;
  bool started_ = false;
  void send(uint8_t keycode);
public:
  // Set from the Bluedroid (BT) task by the server callbacks; read by the main loop.
  std::atomic<bool> connected_{false}, active_{false}, readvertise_{false};
  static constexpr uint8_t rightArrow = 0x4F, leftArrow = 0x50;
  void begin();              // First call initializes Bluedroid; later calls only resume advertising.
  void end();                // Disconnects and stops advertising. Never deinitializes Bluedroid.
  void poll();               // Main loop: restarts advertising after a disconnect.
  bool started() const { return started_; }
  bool linked() const { return connected_.load(); }  // GATT connected.
  void resetSubscription();  // BT task: forget the input report CCCD on (dis)connect.
  bool ready() const;        // Linked and the host subscribed to input report notifications.
  void tap(uint8_t keycode); // Press then release; ignored unless ready().
};
}
