#include "hid_report.hpp"
namespace mini {
HidPayload hidPayload(HidReport report, uint8_t code) {
  HidPayload payload;
  if (report == HidReport::Keyboard) { payload.size = 8; payload.bytes[2] = code; } // modifier, reserved, key[0].
  else { payload.size = 1; payload.bytes[0] = code; }
  return payload;
}
void HidSender::tap(const HidLink& link, HidPort& port, HidReport report, uint8_t code) {
  if (busy() || !link.ready(report)) return;
  const uint32_t session = link.session();
  const uint16_t connId = link.connId();
  const auto press = hidPayload(report, code), release = hidPayload(report, 0);
  if (port.send(connId, report, press.bytes.data(), press.size)) {
    port.wait(hidReleaseDelayMs);
    // Once the link is gone the host has released the key; nothing is resent to a later host.
    if (link.linked() && link.session() == session && !port.send(connId, report, release.bytes.data(), release.size)) {
      state_ = State::ReleasePending; report_ = report; session_ = session; dueAt_ = port.now() + hidRecoveryMs;
    }
  }
  // Never leave a press staged for a host that reconnects later.
  port.store(report, release.bytes.data(), release.size);
}
void HidSender::disconnect(const HidLink& link, HidPort& port) {
  if (!link.linked()) return;
  state_ = State::Disconnecting; session_ = link.session(); dueAt_ = port.now() + hidRecoveryMs;
  port.disconnect(link.connId());
}
void HidSender::poll(const HidLink& link, HidPort& port) {
  if (state_ == State::Idle) return;
  if (!link.linked() || link.session() != session_) { state_ = State::Idle; return; } // Host released on disconnect.
  const uint32_t now = port.now();
  if (int32_t(now - dueAt_) < 0) return;
  dueAt_ = now + hidRecoveryMs;
  if (state_ == State::ReleasePending) {
    const auto release = hidPayload(report_, 0);
    if (port.send(link.connId(), report_, release.bytes.data(), release.size)) { state_ = State::Idle; return; }
    state_ = State::Disconnecting; // The one retry failed: a disconnect makes the host release every key.
  }
  port.disconnect(link.connId()); // Repeated each interval until the disconnect event arrives.
}
void HidRemote::poll(HidPort& port, bool wanted) {
  if (wanted && !active_) {
    active_ = true;
    if (!started_) { port.start(); started_ = true; }
    readvertise_ = false;
    if (!link.linked()) port.advertise(true); // Otherwise the old link's disconnect event restarts it.
  } else if (!wanted && active_) {
    active_ = false;
    port.advertise(false);
    sender_.disconnect(link, port);
  }
  if (!started_) return;
  // Extra connections are never sent to; close them, again each interval until their events arrive.
  const bool extras = link.extra(0) != HidLink::none || link.extra(1) != HidLink::none;
  const uint32_t now = port.now();
  if (!extras) closingExtras_ = false;
  else if (!closingExtras_ || int32_t(now - extrasDueAt_) >= 0) {
    closingExtras_ = true; extrasDueAt_ = now + hidRecoveryMs;
    for (size_t i = 0; i < 2; ++i) if (const uint16_t id = link.extra(i); id != HidLink::none) port.disconnect(id);
  }
  if (readvertise_.exchange(false) && active_ && !link.linked()) port.advertise(true);
  // A host that connected just as advertising stopped is not kept either.
  if (!active_ && link.linked() && !sender_.busy()) sender_.disconnect(link, port);
  sender_.poll(link, port);
}
}
