#include "ble_keyboard.hpp"
#include <Arduino.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEHIDDevice.h>
#include <BLESecurity.h>
#include <BLE2902.h>
#include <HIDTypes.h>
#include <functional>
namespace mini {
namespace {
// The keyboard map as originally built from HIDTypes.h; guards the raw copy in hid_report.hpp.
constexpr uint8_t keyboardMap[] = {
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
constexpr bool keyboardMapMatches(size_t i = 0) {
  return i == sizeof(keyboardMap) || (keyboardMap[i] == hidReportMap[i] && keyboardMapMatches(i + 1));
}
static_assert(sizeof(keyboardMap) == hidKeyboardMapSize && keyboardMapMatches(), "keyboard report map changed");
// Captured on the BT task from the registration event; read by the main loop to send and close.
std::atomic<uint8_t> gattsIf{ESP_GATT_IF_NONE};
// Set once in start(), before any connection.
HidLink* link = nullptr;
uint16_t cccdHandle[2] = {};
// BT task. Mirrors CCCD writes into the link's atomics; the main loop never reads the descriptor
// buffer. Seen here rather than in a descriptor callback because only this event carries conn_id,
// so an extra connection cannot change the current link's subscriptions.
void onGattsEvent(esp_gatts_cb_event_t event, esp_gatt_if_t gatts_if, esp_ble_gatts_cb_param_t* param) {
  if (event == ESP_GATTS_REG_EVT && param->reg.status == ESP_GATT_OK) gattsIf = gatts_if;
  if (event != ESP_GATTS_WRITE_EVT || !link || param->write.is_prep || param->write.len < 1) return;
  for (auto report : {HidReport::Keyboard, HidReport::Consumer})
    if (param->write.handle == cccdHandle[unsigned(report)])
      link->subscribe(param->write.conn_id, report, (param->write.value[0] & 0x01) != 0);
}
// Runs on the BT task: only updates the link. Advertising restarts from poll() on the main loop.
// The library calls the plain overloads first and then these; only these carry conn_id.
class Callbacks : public BLEServerCallbacks {
  BleKeyboard& owner_;
public:
  explicit Callbacks(BleKeyboard& owner) : owner_(owner) {}
  void onConnect(BLEServer*, esp_ble_gatts_cb_param_t* param) override {
    if (owner_.remote_.link.connect(param->connect.conn_id)) owner_.resetSubscription();
  }
  void onDisconnect(BLEServer*, esp_ble_gatts_cb_param_t* param) override {
    if (owner_.remote_.disconnected(param->disconnect.conn_id)) owner_.resetSubscription();
  }
};
}
void BleKeyboard::start() {
  link = &remote_.link;
  BLEDevice::init("StickC Remote");
  BLEDevice::setCustomGattsHandler(onGattsEvent); // Before createServer(), which registers the app.
  auto* server = BLEDevice::createServer();
  server->setCallbacks(new Callbacks(*this));
  auto* hid = new BLEHIDDevice(server);
  BLECharacteristic* reports[2] = {hid->inputReport(1), hid->inputReport(2)};
  // start() creates characteristics in pointer order (std::map), so bonded hosts would see the two
  // 0x2A4D handles swap between boots. Give the lower address the keyboard report every time.
  // The order against the other HID characteristics still follows addresses, as before.
  if (std::less<BLECharacteristic*>()(reports[1], reports[0])) std::swap(reports[0], reports[1]);
  for (auto report : {HidReport::Keyboard, HidReport::Consumer}) {
    const unsigned i = unsigned(report);
    input_[i] = reports[i];
    uint8_t reference[2] = {hidReportId(report), 0x01}; // Report Reference: ID, input.
    input_[i]->getDescriptorByUUID(BLEUUID(uint16_t(0x2908)))->setValue(reference, sizeof(reference));
    cccd_[i] = static_cast<BLE2902*>(input_[i]->getDescriptorByUUID(BLEUUID(uint16_t(0x2902))));
    const auto release = hidPayload(report, 0);
    store(report, release.bytes.data(), release.size);
  }
  // The string overload dereferences an uncreated characteristic; create it first.
  hid->manufacturer()->setValue("M5Stack");
  hid->pnp(0x02, 0xe502, 0xa111, 0x0210);
  hid->hidInfo(0x00, 0x01);
  hid->reportMap(const_cast<uint8_t*>(hidReportMap.data()), hidReportMap.size()); // Copied into the characteristic.
  hid->startServices();
  for (unsigned i = 0; i < 2; ++i) cccdHandle[i] = cccd_[i]->getHandle();
  Serial.printf("BLE: report 1 handle=%u, report 2 handle=%u\n", input_[0]->getHandle(), input_[1]->getHandle());
  auto* security = new BLESecurity();
  security->setAuthenticationMode(ESP_LE_AUTH_BOND); // Just Works: no PIN on the PC.
  security->setCapability(ESP_IO_CAP_NONE);
  security->setInitEncryptionKey(ESP_BLE_ENC_KEY_MASK | ESP_BLE_ID_KEY_MASK);
  auto* advertising = BLEDevice::getAdvertising();
  advertising->setAppearance(HID_KEYBOARD);
  advertising->addServiceUUID(hid->hidService()->getUUID());
  advertising->setScanResponse(true);
}
void BleKeyboard::advertise(bool on) {
  if (!on) { BLEDevice::stopAdvertising(); return; }
  BLEDevice::startAdvertising();
  Serial.println("BLE: advertising as StickC Remote");
}
void BleKeyboard::resetSubscription() {
  // BT task only, like the CCCD writes; the main loop reads the atomics in the link instead.
  uint8_t zero[2] = {0, 0};
  for (auto* cccd : cccd_) if (cccd) cccd->setValue(zero, sizeof(zero));
}
bool BleKeyboard::send(uint16_t connId, HidReport report, const uint8_t* data, size_t size) {
  // Sent directly instead of notify(), which reads the CCCD and the server's peer map unsynchronized.
  // ESP_OK means the local stack queued the packet, not that the host received it.
  store(report, data, size);
  const uint8_t gatts = gattsIf;
  return gatts != ESP_GATT_IF_NONE && esp_ble_gatts_send_indicate(gatts, connId, input_[unsigned(report)]->getHandle(),
    size, const_cast<uint8_t*>(data), false) == ESP_OK;
}
void BleKeyboard::store(HidReport report, const uint8_t* data, size_t size) {
  // Returned when the host reads it. The library's read on the BT task is not synchronized with this
  // write (as before this change); only the CCCD and peer-map races were removed.
  input_[unsigned(report)]->setValue(const_cast<uint8_t*>(data), size);
}
void BleKeyboard::disconnect(uint16_t connId) {
  const uint8_t gatts = gattsIf;
  const esp_err_t rc = gatts == ESP_GATT_IF_NONE ? ESP_FAIL : esp_ble_gatts_close(gatts, connId);
  if (rc != ESP_OK) Serial.printf("BLE: disconnect request failed (rc=%d)\n", int(rc));
}
void BleKeyboard::wait(uint32_t ms) { delay(ms); }
uint32_t BleKeyboard::now() { return millis(); }
}
