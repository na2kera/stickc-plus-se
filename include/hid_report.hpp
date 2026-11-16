#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
namespace mini {
// Hardware-independent part of the BLE HID remote: report map, report bytes,
// per-report subscription state and the press/release sequence.
enum class HidReport : uint8_t { Keyboard, Consumer }; // Report ID 1, Report ID 2.
constexpr uint8_t hidReportId(HidReport report) { return report == HidReport::Keyboard ? 1 : 2; }
constexpr size_t hidKeyboardMapSize = 47;
inline constexpr std::array<uint8_t, 72> hidReportMap = {
  // Keyboard, Report ID 1: modifier bits, reserved byte, key[6].
  0x05, 0x01, 0x09, 0x06, 0xA1, 0x01, 0x85, 0x01,
  0x05, 0x07, 0x19, 0xE0, 0x29, 0xE7,
  0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x08,
  0x81, 0x02,
  0x95, 0x01, 0x75, 0x08, 0x81, 0x01,
  0x95, 0x06, 0x75, 0x08, 0x15, 0x00, 0x25, 0x65,
  0x05, 0x07, 0x19, 0x00, 0x29, 0x65,
  0x81, 0x00,
  0xC0,
  // Consumer Control, Report ID 2: Volume Increment bit, 7 padding bits.
  0x05, 0x0C, 0x09, 0x01, 0xA1, 0x01, 0x85, 0x02,
  0x15, 0x00, 0x25, 0x01, 0x75, 0x01, 0x95, 0x01,
  0x09, 0xE9, 0x81, 0x02,
  0x95, 0x07, 0x81, 0x01,
  0xC0
};
constexpr uint8_t hidVolumeUp = 0x01; // Consumer report bit 0.
struct HidPayload { std::array<uint8_t, 8> bytes{}; uint8_t size = 0; };
HidPayload hidPayload(HidReport report, uint8_t code); // code 0 is the release report.
// Written on the BT task (connect, disconnect, CCCD write); read on the main loop.
class HidLink {
  std::atomic<bool> connected_{false}, keyboard_{false}, consumer_{false};
  std::atomic<uint16_t> connId_{0};
  std::atomic<uint32_t> session_{0}; // Advances on every connect.
public:
  static constexpr uint16_t none = 0xFFFF;
private:
  // Connections that arrived while one was linked (the SDK allows 3); the main loop closes them.
  std::atomic<uint16_t> extra_[2] = {{none}, {none}};
  std::atomic<bool>& flag(HidReport report) { return report == HidReport::Keyboard ? keyboard_ : consumer_; }
public:
  // One link at a time: another connection leaves the current link untouched and is parked to be closed.
  bool connect(uint16_t connId) {
    if (connected_ && connId != connId_) {
      for (auto& extra : extra_) { uint16_t free = none; if (extra.compare_exchange_strong(free, connId)) break; }
      return false;
    }
    connId_ = connId; keyboard_ = consumer_ = false; ++session_; connected_ = true; return true;
  }
  // Only the current connection's event counts; a stale or extra one must not wipe the current link.
  bool disconnect(uint16_t connId) {
    for (auto& extra : extra_) { uint16_t id = connId; if (extra.compare_exchange_strong(id, none)) return false; }
    if (!connected_ || connId != connId_) return false;
    connected_ = false; keyboard_ = consumer_ = false; return true;
  }
  void subscribe(HidReport report, bool notifications) { flag(report) = notifications; }
  void subscribe(uint16_t connId, HidReport report, bool notifications) { // CCCD write from a host.
    if (connected_ && connId == connId_) subscribe(report, notifications);
  }
  uint16_t extra(size_t i) const { return extra_[i].load(); }
  bool linked() const { return connected_.load(); }
  uint16_t connId() const { return connId_.load(); }
  uint32_t session() const { return session_.load(); }
  bool subscribed(HidReport report) const { return (report == HidReport::Keyboard ? keyboard_ : consumer_).load(); }
  bool ready(HidReport report) const { return connected_ && subscribed(report); }
};
// SDK side, called from the main loop only.
class HidPort {
public:
  virtual void start() = 0;                 // Brings up the stack and services once, without advertising.
  virtual void advertise(bool on) = 0;
  // Stores the value, then sends it. true means the local stack accepted it, not that the host received it.
  virtual bool send(uint16_t connId, HidReport report, const uint8_t* data, size_t size) = 0;
  virtual void store(HidReport report, const uint8_t* data, size_t size) = 0; // Stores without sending.
  virtual void disconnect(uint16_t connId) = 0; // Asks the stack to close; the event may never come.
  virtual void wait(uint32_t ms) = 0;
  virtual uint32_t now() = 0;
protected:
  ~HidPort() = default;
};
constexpr uint32_t hidReleaseDelayMs = 8; // Provisional until checked on an iPhone.
// Several iOS connection intervals (15-30 ms), so queued packets can drain between attempts.
constexpr uint32_t hidRecoveryMs = 100;
// Main loop only. Keeps a failed release or a requested disconnect alive across calls until the
// link of that session is gone; no new press is sent meanwhile.
class HidSender {
  enum class State : uint8_t { Idle, ReleasePending, Disconnecting } state_ = State::Idle;
  HidReport report_ = HidReport::Keyboard;
  uint32_t session_ = 0, dueAt_ = 0;
public:
  bool busy() const { return state_ != State::Idle; }
  bool releasePending() const { return state_ == State::ReleasePending; }
  void tap(const HidLink& link, HidPort& port, HidReport report, uint8_t code);
  void disconnect(const HidLink& link, HidPort& port);
  void poll(const HidLink& link, HidPort& port);
};
// Screen enter/leave, advertising and recovery. One link at a time: advertising waits until the
// previous link's disconnect event, and a connection that still arrives next to it is closed.
class HidRemote {
  bool started_ = false, active_ = false, closingExtras_ = false; // Main loop only.
  uint32_t extrasDueAt_ = 0;
  std::atomic<bool> readvertise_{false};  // Set on the BT task by the current link's disconnect.
  HidSender sender_;
public:
  HidLink link; // Its connect/subscribe are called directly from the BT task.
  // BT task. false for an event of another connection. Together with !link.linked() in poll() this keeps
  // advertising from restarting twice; each guard is tested on its own.
  bool disconnected(uint16_t connId) {
    if (!link.disconnect(connId)) return false;
    readvertise_ = true; return true;
  }
  void poll(HidPort& port, bool wanted); // Every loop, whether or not the screen wants BLE.
  void tap(HidPort& port, HidReport report, uint8_t code) { if (active_) sender_.tap(link, port, report, code); }
  bool busy() const { return sender_.busy(); }
};
}
