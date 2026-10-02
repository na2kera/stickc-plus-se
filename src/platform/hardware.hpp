#pragma once
#include <M5Unified.h>
#include <Preferences.h>
#include "model.hpp"
namespace mini {
class Hardware {
  Preferences preferences_;
  bool storageReady_ = false, micActive_ = false, micPending_ = false, micFailed_ = false;
  uint32_t micQueuedAt_ = 0;
  int16_t samples_[cfg::micSamples]{};
public:
  Input input;
  M5Canvas canvas{&M5.Display};
  bool storageError = false, canvasReady = false, boardReady = false;
  void begin(Records& records);
  void save(const Records& records);
  MicFrame pollMic(bool wanted, uint32_t now);
  void sound(Sound sound, bool enabled, bool muted);
};
void render(Hardware& hardware, const App& app, uint32_t now);
}
