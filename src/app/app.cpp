#include "model.hpp"
namespace mini {
void App::transition(Screen next, uint32_t now) {
  screen = next; enteredAt_ = now; lastActivity_ = now; gate_ = true;
}
bool App::wantsMic() const {
  return screen == Screen::CalQuiet || screen == Screen::CalVoice ||
    (game == Game::Flight && mode == Mode::Voice && (screen == Screen::Countdown || screen == Screen::Play));
}
void App::beginCalibration(uint32_t now) {
  mode = Mode::Voice; calibration.reset(); lastMicAt_ = now; transition(Screen::CalQuiet, now);
}
void App::start(uint32_t now) {
  result = {}; result.mode = mode;
  switch (game) {
    case Game::Baseball: baseball = {}; baseball.pitch(now, random); break;
    case Game::Balloon:
#ifdef MINIGAMES_BURST_STAGE
      balloon.enter(now, random, MINIGAMES_BURST_STAGE);
#else
      balloon.enter(now, random);
#endif
      break;
    case Game::Memory: memory.enter(now, random); break;
    case Game::Flight: flight.enter(now); lastMicAt_ = now; break;
    case Game::Clock: clock = {}; break;
  }
  transition(Screen::Play, now); sound_ = Sound::Start;
}
void App::finish(uint32_t now) {
  switch (game) {
    case Game::Baseball: result.score = baseball.score; break;
    case Game::Balloon: result.burst = balloon.burst; result.score = balloon.burst ? 0 : balloon.stage * 10; break;
    case Game::Memory: result.score = memory.score; result.perfect = memory.perfect; break;
    case Game::Flight: result.elapsedMs = flight.elapsedMs; result.score = flight.elapsedMs / 100; break;
    case Game::Clock:
      result.elapsedMs = clock.elapsedMs; result.score = clock.error(); result.timeout = clock.timeout; result.eligible = !clock.timeout; break;
  }
  if (result.eligible && records.submit(game, mode, result.score)) { result.newBest = true; storageDirty_ = true; }
  transition(Screen::Result, now);
  sound_ = result.newBest ? Sound::Best : (result.burst || result.timeout || (game == Game::Memory && !result.perfect) || (game == Game::Flight && flight.collision) ? Sound::Failure : Sound::Success);
}
void App::update(uint32_t now, InputFrame input, const MicFrame& mic) {
  if (input.anyEvent() || input.a.down || input.b.down) lastActivity_ = now;
  if ((screen == Screen::Play || screen == Screen::Countdown || screen == Screen::CalQuiet || screen == Screen::CalVoice) && input.abort) {
    calibrationOnly_ = false; transition(Screen::Menu, now); sound_ = Sound::None; return;
  }
  if (input.both) { input.a.pressed = input.b.pressed = input.a.shortPress = input.b.shortPress = false; }
  if (gate_) {
    const bool released = !input.a.down && !input.b.down && !input.both;
    input.a.pressed = input.b.pressed = input.a.shortPress = input.b.shortPress = false;
    if (released) gate_ = false;
  }
  const bool idleScreen = screen == Screen::Menu || screen == Screen::Instructions || screen == Screen::Result || screen == Screen::Settings || screen == Screen::ResetConfirm || screen == Screen::MicError || screen == Screen::CalDone;
  if (idleScreen && uint32_t(now - lastActivity_) >= cfg::idleMs) { selection = 0; calibrationOnly_ = false; transition(Screen::Menu, now); return; }
  if (wantsMic()) {
    if (mic.fresh && mic.ok) lastMicAt_ = now;
    if (!mic.ok || uint32_t(now - lastMicAt_) >= cfg::micStaleMs) { transition(Screen::MicError, now); sound_ = Sound::None; return; }
  }
  switch (screen) {
    case Screen::Menu:
      if (input.a.shortPress) selection = (selection + 1) % 6;
      if (input.b.shortPress) {
        if (selection == 5) { setting = 0; transition(Screen::Settings, now); }
        else { game = Game(selection); mode = records.settings.mode; transition(Screen::Instructions, now); }
      }
      break;
    case Screen::Instructions:
      if (input.b.shortPress) transition(Screen::Menu, now);
      else if (input.a.shortPress) {
        calibrationOnly_ = false;
        if (game == Game::Flight && mode == Mode::Voice) beginCalibration(now);
        else transition(Screen::Countdown, now);
      }
      break;
    case Screen::Countdown:
      if (mic.fresh && mode == Mode::Voice && game == Game::Flight) calibration.normalize(mic.rms);
      if (screenElapsed(now) >= cfg::countdownMs) start(now);
      break;
    case Screen::Play: {
      bool done = false;
      switch (game) {
        case Game::Baseball: {
          const bool before = baseball.feedback;
          done = baseball.update(now, input, random);
          if (!before && baseball.feedback) sound_ = baseball.lastPoints ? Sound::Success : Sound::Failure;
          break;
        }
        case Game::Balloon: done = balloon.update(now, input); break;
        case Game::Memory: {
          const auto before = memory.phase;
          done = memory.update(now, input, random);
          if (before != Memory::Phase::Success && memory.phase == Memory::Phase::Success) sound_ = Sound::Success;
          break;
        }
        case Game::Flight:
          if (mode == Mode::Voice && mic.fresh) calibration.normalize(mic.rms);
          done = flight.update(now, mode == Mode::Button ? (input.a.down && !gate_ && !input.both ? 1.f : 0.f) : calibration.filtered, random);
          break;
        case Game::Clock: done = clock.update(now, input); break;
      }
      if (done) finish(now);
      break;
    }
    case Screen::Result:
      if (input.b.shortPress) transition(Screen::Menu, now);
      else if (input.a.shortPress) {
        if (game == Game::Flight && mode == Mode::Voice) beginCalibration(now);
        else transition(Screen::Countdown, now);
      }
      break;
    case Screen::Settings:
      if (input.a.shortPress) setting = (setting + 1) % 6;
      if (input.b.shortPress) {
        if (setting == 0) { records.settings.sound = !records.settings.sound; storageDirty_ = true; }
        if (setting == 1) { records.settings.brightness = (records.settings.brightness + 1) % 3; storageDirty_ = true; }
        if (setting == 2) { records.settings.mode = records.settings.mode == Mode::Voice ? Mode::Button : Mode::Voice; storageDirty_ = true; }
        if (setting == 3) { game = Game::Flight; calibrationOnly_ = true; beginCalibration(now); }
        if (setting == 4) transition(Screen::ResetConfirm, now);
        if (setting == 5) transition(Screen::Menu, now);
      }
      break;
    case Screen::ResetConfirm:
      if (input.a.shortPress) transition(Screen::Settings, now);
      else if (!gate_ && !input.both && input.b.down && input.b.heldMs >= 2000) { records.reset(); storageDirty_ = true; transition(Screen::Settings, now); }
      break;
    case Screen::CalQuiet:
      if (input.b.shortPress) { beginCalibration(now); break; }
      if (mic.fresh) calibration.sample(mic.rms);
      if (screenElapsed(now) >= cfg::calibrationMs) {
        if (calibration.count < 20) transition(Screen::MicError, now);
        else { calibration.beginVoice(); transition(Screen::CalVoice, now); }
      }
      break;
    case Screen::CalVoice:
      if (input.b.shortPress) { beginCalibration(now); break; }
      if (mic.fresh) calibration.sample(mic.rms);
      if (screenElapsed(now) >= cfg::calibrationMs) {
        if (!calibration.finish()) transition(Screen::MicError, now);
        else transition(calibrationOnly_ ? Screen::CalDone : Screen::Countdown, now);
      }
      break;
    case Screen::CalDone:
      if (input.a.shortPress || input.b.shortPress) { calibrationOnly_ = false; transition(Screen::Settings, now); }
      break;
    case Screen::MicError:
      if (input.a.shortPress) beginCalibration(now);
      else if (input.b.shortPress) {
        if (calibrationOnly_) { calibrationOnly_ = false; transition(Screen::Settings, now); }
        else { mode = Mode::Button; transition(Screen::Countdown, now); }
      }
      break;
  }
}
}
