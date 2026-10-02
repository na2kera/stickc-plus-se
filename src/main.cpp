#include "platform/hardware.hpp"
#include <esp_system.h>
namespace {
mini::App app;
mini::Hardware hardware;
uint32_t lastRender = 0;
}
void setup() {
  hardware.begin(app.records);
#ifdef MINIGAMES_SEED
  app.random = mini::Random(MINIGAMES_SEED);
#else
  app.random = mini::Random(esp_random());
#endif
  mini::render(hardware, app, millis());
}
void loop() {
  M5.update();
  const uint32_t now = millis();
  // Poll at ~1ms independently of the 30fps rendering schedule.
  // GPIOs are initialized by M5Unified. Read raw levels so Input owns the only
  // debounce stage and retains the first stable edge's timestamp.
  auto input = hardware.input.update(digitalRead(37) == LOW, digitalRead(39) == LOW, now);
  const auto mic = hardware.pollMic(hardware.boardReady && app.wantsMic(), now);
  const auto previousScreen = app.screen;
  if (hardware.boardReady) app.update(now, input, mic);
  if (previousScreen != app.screen) M5.Speaker.stop();
  // Stop capture immediately on exit/error; never reuse the buffer before end().
  if (!app.wantsMic()) hardware.pollMic(false, now);
  if (app.takeStorageDirty()) {
    hardware.save(app.records);
    M5.Display.setBrightness(mini::cfg::brightness[app.records.settings.brightness]);
  }
  hardware.sound(app.takeSound(), app.records.settings.sound, app.muted());
  if (uint32_t(now - lastRender) >= 33) { lastRender = now; mini::render(hardware, app, now); }
  delay(1); // scheduler yield; gameplay and display have independent clocks.
}
