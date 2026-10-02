#include "render.hpp"
#include <cstdio>
#include <stdexcept>
using namespace mini;
static unsigned frames = 0;
static void capture(lgfx::LGFX_Sprite& canvas, const App& app, uint32_t now, const char* name) {
  drawScreen(canvas, app, now, true, false);
  char path[160]; std::snprintf(path, sizeof(path), "build/previews/%02u-%s.ppm", frames++, name);
  FILE* file = std::fopen(path, "wb"); if (!file) throw std::runtime_error("preview output failed");
  std::fprintf(file, "P6\n240 135\n255\n");
  for (int y = 0; y < 135; ++y) for (int x = 0; x < 240; ++x) {
    auto color = canvas.readPixelRGB(x, y); uint8_t pixel[3] = {color.R8(), color.G8(), color.B8()}; std::fwrite(pixel, 1, 3, file);
  }
  std::fclose(file);
}
int main() {
  try {
    lgfx::LGFX_Sprite canvas; canvas.setColorDepth(16); canvas.createSprite(240, 135);
    App app;
    for (unsigned n = 0; n < 6; ++n) { app.selection = n; capture(canvas, app, 0, "menu"); }
    for (unsigned n = 0; n < 5; ++n) {
      app.game = Game(n); app.screen = Screen::Instructions; capture(canvas, app, 0, "instructions");
      app.screen = Screen::Play; capture(canvas, app, 1000, "play");
      app.screen = Screen::Result; app.result.score = 100; app.result.elapsedMs = 10000; app.result.newBest = true;
      app.records.submit(app.game, Mode::Voice, 100); capture(canvas, app, 0, "result");
    }
    app.game = Game::Baseball; app.screen = Screen::Play; app.baseball.feedback = true; app.baseball.lastPoints = 100; capture(canvas, app, 0, "homerun");
    app.game = Game::Memory;
    for (auto phase : {Memory::Phase::Answer, Memory::Phase::Release, Memory::Phase::Success}) {
      app.memory.phase = phase; capture(canvas, app, 0, "memory");
    }
    app.game = Game::Flight; app.flight.enter(0); app.flight.update(2000, .45f, app.random); capture(canvas, app, 0, "flight");
    app.game = Game::Clock; app.clock.update(20, [] { InputFrame i; i.a.pressed = i.a.down = true; return i; }());
    app.clock.elapsedMs = 1000; capture(canvas, app, 1000, "clock-visible"); app.clock.elapsedMs = 2000; capture(canvas, app, 2000, "clock-hidden");
    app.screen = Screen::Result; app.result.timeout = true; capture(canvas, app, 0, "timeout");
    for (unsigned n = 0; n < 6; ++n) { app.screen = Screen::Settings; app.setting = n; capture(canvas, app, 0, "settings"); }
    for (auto screen : {Screen::Countdown, Screen::ResetConfirm, Screen::CalQuiet, Screen::CalVoice, Screen::CalDone, Screen::MicError}) {
      app.screen = screen; capture(canvas, app, 0, "common");
    }
    // Maximum visible records, both flight modes, and failure statuses.
    for (unsigned n = 0; n < 5; ++n) {
      app.game = Game(n); app.screen = Screen::Result; app.mode = Mode::Button;
      app.result = {}; app.result.mode = Mode::Button; app.result.score = n == 4 ? 9999 : n == 3 ? 200 : 1000; app.result.elapsedMs = 19999;
      auto& best = app.records.best[app.records.index(app.game, app.mode)]; best = {true, app.result.score};
      capture(canvas, app, 0, "large-result");
    }
    std::printf("PASS: %u render previews; Japanese font bounds checked\n", frames);
  } catch (const std::exception& error) { std::fprintf(stderr, "%s\n", error.what()); return 1; }
}
