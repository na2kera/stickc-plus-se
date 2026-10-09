#include "ble_keyboard.hpp"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <BLE2902.h>
#include <HIDTypes.h>
namespace mini {
namespace {
// Standard 8-byte keyboard input report: modifier, reserved, key[6]. Report ID 1.
uint8_t reportMap[] = {
  USAGE_PAGE(1), 0x01, USAGE(1), 0x06, COLLECTION(1), 0x01, REPORT_ID(1), 0x01,
  USAGE_PAGE(1), 0x07, USAGE_MINIMUM(1), 0xE0, USAGE_MAXIMUM(1), 0xE7,
  LOGICAL_MINIMUM(1), 0x00, LOGICAL_MAXIMUM(1), 0x01, REPORT_SIZE(1), 0x01, REPORT_COUNT(1), 0x08,
  HIDINPUT(1), 0x02,                                   // Modifier bits.
  REPORT_COUNT(1), 0x01, REPORT_SIZE(1), 0x08, HIDINPUT(1), 0x01, // Reserved byte.
  REPORT_COUNT(1), 0x06, REPORT_SIZE(1), 0x08, LOGICAL_MINIMUM(1), 0x00, LOGICAL_MAXIMUM(1), 0x65,
  USAGE_PAGE(1), 0x07, USAGE_MINIMUM(1), 0x00, USAGE_MAXIMUM(1), 0x65,
  HIDINPUT(1), 0x00,                                   // Key array.
  END_COLLECTION(0)
};
// Runs on the BT task: only sets flags. Advertising restarts from poll() on the main loop.
class Callbacks : public BLEServerCallbacks {
  BleKeyboard& owner_;
public:
  explicit Callbacks(BleKeyboard& owner) : owner_(owner) {}
  void onConnect(BLEServer*) override { owner_.resetSubscription(); owner_.connected_ = true; }
  void onDisconnect(BLEServer*) override {
    owner_.connected_ = false; owner_.resetSubscription(); owner_.readvertise_ = true;
  }
};
}
void BleKeyboard::begin() {
  active_ = true;
  if (started_) {
    readvertise_ = false; // Ignore a late disconnect event from the previous end().
    BLEDevice::startAdvertising(); return;
  }
  BLEDevice::init("StickC Remote");
  server_ = BLEDevice::createServer();
  server_->setCallbacks(new Callbacks(*this));
  auto* hid = new BLEHIDDevice(server_);
  input_ = hid->inputReport(1);
  inputCccd_ = static_cast<BLE2902*>(input_->getDescriptorByUUID(BLEUUID(uint16_t(0x2902))));
  // The string overload dereferences an uncreated characteristic; create it first.
  hid->manufacturer()->setValue("M5Stack");
  hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
  hid->hidInfo(0x00, 0x01);
  hid->reportMap(reportMap, sizeof(reportMap));
  hid->startServices();
  auto* security = new BLESecurity();
  security->setAuthenticationMode(ESP_LE_AUTH_BOND); // Just Works: no PIN on the PC.
  security->setCapability(ESP_IO_CAP_NONE);
  security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  auto* advertising = BLEDevice::getAdvertising();
  advertising->setAppearance(HID_KEYBOARD);
  advertising->addServiceUUID(hid->hidService()->getUUID());
  advertising->setScanResponse(true);
  advertising->start();
  started_ = true;
  Serial.println("BLE: advertising as StickC Remote");
}
void BleKeyboard::end() {
  active_ = false;
  if (!started_) return;
  BLEDevice::stopAdvertising();
  if (connected_) server_->disconnect(server_->getConnId());
  readvertise_ = false;
}
void BleKeyboard::poll() {
  if (readvertise_.exchange(false) && active_) BLEDevice::startAdvertising();
}
void BleKeyboard::resetSubscription() {
  // Written on the BT task; ready() reads one byte of the same fixed buffer.
  uint8_t zero[2] = {0, 0};
  if (inputCccd_) inputCccd_->setValue(zero, sizeof(zero));
}
bool BleKeyboard::ready() const {
  return connected_ && inputCccd_ && inputCccd_->getNotifications();
}
void BleKeyboard::send(uint8_t keycode) {
  uint8_t report[8] = {0, 0, keycode, 0, 0, 0, 0, 0};
  input_->setValue(report, sizeof(report)); input_->notify();
}
void BleKeyboard::tap(uint8_t keycode) {
  if (!started_ || !input_ || !ready()) return;
  send(keycode); delay(8);
  // notify() copies BLEServer's peer map without a lock while the BT task may erase
  // from it on disconnect. The library cannot be changed, so re-check to narrow that
  // window; a residual library-level race remains.
  if (connected_) send(0);
}
}
