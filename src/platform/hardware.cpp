#include "hardware.hpp"
#include "render.hpp"
namespace mini {
void Hardware::begin(Records& records) {
  auto config = M5.config();
  config.serial_baudrate = 115200;
  config.internal_imu = false; config.internal_rtc = false;
  config.internal_mic = true; config.internal_spk = true;
  // Factory's active M5GFX detection and the documented SE pinmap select StickCPlus.
  // Do not force Plus2 or substitute a fallback display for an unknown device.
  M5.begin(config);
  M5.Display.setRotation(3);
  boardReady = M5.getBoard() == m5::board_t::board_M5StickCPlus && M5.Display.width() == cfg::width && M5.Display.height() == cfg::height;
  Serial.printf("SE: board=%d display=%dx%d A=37 B=39 buzzer=2 mic=0/34\n", int(M5.getBoard()), M5.Display.width(), M5.Display.height());
  M5.Mic.end(); M5.Speaker.end();
  auto micConfig = M5.Mic.config();
  micConfig.sample_rate = cfg::micRate; micConfig.over_sampling = 1;
  micConfig.noise_filter_level = 0;
  M5.Mic.config(micConfig);
  storageReady_ = preferences_.begin("minigames", false);
  records = {};
  if (storageReady_) {
    const size_t size = preferences_.getBytesLength("state");
    if (size) {
      StorageBytes bytes{};
      if (size != bytes.size() || preferences_.getBytes("state", bytes.data(), bytes.size()) != bytes.size() || !decode(bytes, records)) {
        storageError = true; Serial.println("NVS: invalid/old data; RAM defaults selected");
      }
    }
  } else { storageError = true; Serial.println("NVS: open failed; games continue in RAM"); }
  M5.Display.setBrightness(cfg::brightness[records.settings.brightness]);
  canvas.setColorDepth(16); canvas.setPsram(false);
  canvasReady = canvas.createSprite(cfg::width, cfg::height) != nullptr;
  if (!canvasReady) Serial.println("Canvas: allocation failed; direct display fallback");
}
void Hardware::save(const Records& records) {
  const auto bytes = encode(records);
  storageError = !storageReady_ || preferences_.putBytes("state", bytes.data(), bytes.size()) != bytes.size();
  Serial.println(storageError ? "NVS: write failed; RAM record retained" : "NVS: state saved");
}
MicFrame Hardware::pollMic(bool wanted, uint32_t now) {
  MicFrame frame;
  if (!wanted) {
    if (micActive_) M5.Mic.end(); // end joins the capture task before buffer reuse.
    micActive_ = micPending_ = micFailed_ = false;
    return frame;
  }
  if (micFailed_) { frame.ok = false; return frame; }
  if (!micActive_) {
    M5.Speaker.stop(); M5.Speaker.end();
    micActive_ = M5.Mic.begin();
    if (!micActive_) { micFailed_ = true; frame.ok = false; return frame; }
  }
  if (micPending_) {
    if (M5.Mic.isRecording()) {
      if (uint32_t(now - micQueuedAt_) >= cfg::micStaleMs) {
        micFailed_ = true; frame.ok = false;
      }
      return frame;
    }
    // Single buffer, single outstanding request. The capture task has released it.
    frame.rms = rmsWithoutDC(samples_, cfg::micSamples); frame.fresh = true; micPending_ = false;
  }
  if (!M5.Mic.record(samples_, cfg::micSamples, cfg::micRate)) { micFailed_ = true; frame.ok = false; }
  else { micPending_ = true; micQueuedAt_ = now; }
  return frame;
}
void Hardware::sound(Sound sound, bool enabled, bool muted) {
  if (!enabled || muted || micActive_) { M5.Speaker.stop(); return; }
  if (sound == Sound::None) return;
  if (!M5.Speaker.begin()) return;
  M5.Speaker.setVolume(96);
  switch (sound) {
    case Sound::Start: M5.Speaker.tone(1100, 70); break;
    case Sound::Success: M5.Speaker.tone(1800, 90); break;
    case Sound::Failure: M5.Speaker.tone(350, 130); break;
    case Sound::Best: M5.Speaker.tone(2400, 160); break;
    case Sound::None: break;
  }
}
void render(Hardware& hardware, const App& app, uint32_t now) {
  lgfx::LGFXBase& canvas = hardware.canvasReady ? static_cast<lgfx::LGFXBase&>(hardware.canvas) : static_cast<lgfx::LGFXBase&>(M5.Display);
  drawScreen(canvas, app, now, hardware.boardReady, hardware.storageError);
  if (hardware.canvasReady) hardware.canvas.pushSprite(0, 0);
}
}
