#pragma once
#include "config.hpp"
namespace mini {
struct ButtonEvent {
  bool down = false, pressed = false, released = false, shortPress = false;
  uint32_t pressedAt = 0, heldMs = 0, releasedAt = 0;
  bool timestamped = false;
};
struct InputFrame {
  ButtonEvent a, b;
  bool abort = false, both = false;
  bool anyEvent() const { return a.pressed || b.pressed || a.released || b.released; }
};
class Input {
  struct Key {
    bool raw = false, stable = false, consumed = false;
    uint32_t changedAt = 0, pressedAt = 0;
    ButtonEvent update(bool value, uint32_t now) {
      ButtonEvent out;
      out.timestamped = true;
      if (value != raw) { raw = value; changedAt = now; }
      if (raw != stable && uint32_t(now - changedAt) >= cfg::debounceMs) {
        stable = raw;
        if (stable) { pressedAt = changedAt; consumed = false; out.pressed = true; }
        else {
          out.released = true;
          out.releasedAt = changedAt;
          out.shortPress = !consumed && uint32_t(changedAt - pressedAt) < cfg::abortMs;
        }
      }
      out.down = stable;
      out.pressedAt = pressedAt;
      out.heldMs = stable ? uint32_t(now - pressedAt) : 0;
      if (out.heldMs >= cfg::abortMs) consumed = true;
      return out;
    }
  } a_, b_;
  bool chord_ = false, aborted_ = false, previousBoth_ = false;
  uint32_t chordAt_ = 0;
public:
  InputFrame update(bool a, bool b, uint32_t now) {
    InputFrame out;
    out.a = a_.update(a, now); out.b = b_.update(b, now);
    out.both = out.a.down && out.b.down;
    if (out.both) {
      if (!previousBoth_) { chord_ = true; chordAt_ = now; aborted_ = false; }
      a_.consumed = b_.consumed = true;
      if (!aborted_ && uint32_t(now - chordAt_) >= cfg::abortMs) {
        out.abort = true; aborted_ = true;
      }
    }
    previousBoth_ = out.both;
    if (chord_) {
      out.a.pressed = out.b.pressed = out.a.shortPress = out.b.shortPress = false;
      // Consume the whole chord, including staggered releases.
      if (!out.a.down && !out.b.down) chord_ = false;
    }
    return out;
  }
};
}
