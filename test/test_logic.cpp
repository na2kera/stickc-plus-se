#include "model.hpp"
#include <cstdio>
#include <cstdlib>
#include <limits>
using namespace mini;
static unsigned assertions = 0;
#define CHECK(expr) do { ++assertions; if (!(expr)) { std::fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #expr); std::exit(1); } } while (0)
static InputFrame shortA() { InputFrame i; i.a.shortPress = i.a.released = true; return i; }
static InputFrame shortB() { InputFrame i; i.b.shortPress = i.b.released = true; return i; }
static InputFrame pressA(uint32_t at) { InputFrame i; i.a.down = i.a.pressed = true; i.a.pressedAt = at; return i; }
static void testInput() {
  Input input;
  CHECK(!input.update(true, false, 100).a.pressed);
  CHECK(!input.update(false, false, 110).a.pressed);
  CHECK(!input.update(true, false, 115).a.pressed);
  CHECK(!input.update(true, false, 134).a.pressed);
  auto i = input.update(true, false, 135); CHECK(i.a.pressed); CHECK(i.a.pressedAt == 115);
  CHECK(!input.update(true, false, 136).a.pressed);
  input.update(false, false, 200); i = input.update(false, false, 220);
  CHECK(i.a.shortPress && i.a.released); CHECK(!input.update(false, false, 221).a.shortPress);
  input.update(true, false, 300); input.update(true, false, 320); input.update(true, false, 1100);
  input.update(false, false, 1200); CHECK(!input.update(false, false, 1220).a.shortPress);
  input.update(true, true, 1300); i = input.update(true, true, 1320);
  CHECK(i.both && !i.a.pressed && !i.b.pressed);
  CHECK(!input.update(true, true, 2119).abort); CHECK(input.update(true, true, 2120).abort);
  CHECK(!input.update(true, true, 2121).abort);
  input.update(false, true, 2200); CHECK(!input.update(false, true, 2220).a.shortPress);
  input.update(false, false, 2300); CHECK(!input.update(false, false, 2320).b.shortPress);
  input.update(false, true, 2400); CHECK(input.update(false, true, 2420).b.pressed);
  Input wrap; wrap.update(true, false, UINT32_MAX - 9); CHECK(wrap.update(true, false, 10).a.pressed);
  Input interrupted;
  interrupted.update(true, true, 0); interrupted.update(true, true, 20);
  interrupted.update(false, true, 100); interrupted.update(false, true, 120);
  interrupted.update(true, true, 1000); CHECK(!interrupted.update(true, true, 1020).abort);
  CHECK(!interrupted.update(true, true, 1819).abort); CHECK(interrupted.update(true, true, 1820).abort);
}
static void testBaseball() {
  for (int sign : {-1, 1}) {
    CHECK(Baseball::points(sign * 50) == 100); CHECK(Baseball::points(sign * 51) == 50);
    CHECK(Baseball::points(sign * 100) == 50); CHECK(Baseball::points(sign * 101) == 20);
    CHECK(Baseball::points(sign * 150) == 20); CHECK(Baseball::points(sign * 151) == 0);
  }
  CHECK(Baseball::points(INT32_MIN) == 0);
  Random random(4); Baseball game; game.pitch(0, random);
  auto arrival = game.pitchAt + game.waitMs + game.travelMs;
  CHECK(!game.update(arrival, pressA(arrival), random)); CHECK(game.score == 100);
  game.update(arrival + 50, pressA(arrival + 50), random); CHECK(game.score == 100);
  CHECK(!game.update(arrival + 600, {}, random)); CHECK(game.ball == 2);
  game.update(game.pitchAt + 10, pressA(game.pitchAt + 10), random); CHECK(game.lastPoints == 0);
  for (unsigned ball = 3; ball <= 10; ++ball) {
    game.update(game.feedbackAt + 600, {}, random); CHECK(game.ball == ball);
    arrival = game.pitchAt + game.waitMs + game.travelMs;
    game.update(arrival + 20, pressA(arrival), random);
  }
  CHECK(game.update(game.feedbackAt + 600, {}, random)); CHECK(game.score == 900);
  Baseball late; late.pitch(100, random); arrival = late.pitchAt + late.waitMs + late.travelMs;
  late.update(arrival + 151, {}, random); CHECK(!late.feedback);
  late.update(arrival + 170, pressA(arrival + 150), random); CHECK(late.lastPoints == 20);
  Baseball miss; miss.pitch(UINT32_MAX - 500, random); arrival = miss.pitchAt + miss.waitMs + miss.travelMs;
  miss.update(arrival + 171, {}, random); CHECK(miss.feedback && miss.score == 0);
  Baseball wrap; wrap.pitch(UINT32_MAX - 500, random); arrival = wrap.pitchAt + wrap.waitMs + wrap.travelMs;
  wrap.update(arrival + 20, pressA(arrival), random); CHECK(wrap.score == 100);
  Input raw; Baseball actual; actual.pitch(0, random); arrival = actual.waitMs + actual.travelMs;
  raw.update(true, false, arrival + 150);
  for (unsigned at = 151; at <= 170; ++at) actual.update(arrival + at, raw.update(true, false, arrival + at), random);
  CHECK(actual.lastPoints == 20);
}
static void testBalloon() {
  Random random(10);
  for (unsigned limit : {12u, 24u}) {
    Balloon b; b.enter(100, random, limit);
    for (unsigned n = 1; n < limit; ++n) CHECK(!b.update(100 + n, shortA()));
    CHECK(b.stage == limit - 1); CHECK(b.update(200, shortA())); CHECK(b.burst);
  }
  Balloon b; b.enter(0, random, 16); auto held = pressA(0); b.update(20, held); CHECK(b.stage == 0);
  b.update(100, shortA()); CHECK(b.update(200, shortB())); CHECK(b.stage == 1 && !b.burst);
  b.enter(UINT32_MAX - 100, random); CHECK(b.burstAt >= 12 && b.burstAt <= 24);
  CHECK(!b.update(b.startedAt + 14999, {})); CHECK(b.update(b.startedAt + 15020, shortA())); CHECK(b.stage == 0);
  Balloon late; late.enter(0, random, 16);
  auto release = shortA(); release.a.timestamped = true; release.a.releasedAt = 14999;
  CHECK(!late.update(15000, {})); late.update(15019, release); CHECK(late.stage == 1);
  CHECK(late.update(15020, {}));
  Balloon after; after.enter(0, random, 16); release.a.releasedAt = 15000;
  after.update(15020, release); CHECK(after.stage == 0);
  bool seen[13]{}; for (unsigned n = 0; n < 10000; ++n) { b.enter(0, random); CHECK(b.burstAt >= 12 && b.burstAt <= 24); seen[b.burstAt - 12] = true; }
  for (bool value : seen) CHECK(value);
}
static void testMemory() {
  Random random(12); Memory m; m.enter(0, random);
  CHECK(m.length == 2); CHECK(m.symbol(0) == m.sequence[0]); CHECK(m.symbol(449) == m.sequence[0]);
  CHECK(m.symbol(450) == -1); CHECK(m.symbol(599) == -1); CHECK(m.symbol(600) == m.sequence[1]);
  auto held = pressA(0); m.update(1200, held, random); CHECK(m.phase == Memory::Phase::Release);
  m.update(1201, held, random); CHECK(m.phase == Memory::Phase::Release);
  m.update(1210, shortA(), random); CHECK(m.phase == Memory::Phase::Answer && m.cursor == 0);
  const auto prefix = m.sequence;
  uint32_t now = 1220;
  while (m.length <= 12) {
    for (unsigned n = 0; n < m.length; ++n) {
      m.update(now, m.sequence[n] ? shortB() : shortA(), random); now += 10;
    }
    CHECK(m.score == m.length);
    if (m.length == 12) break;
    const unsigned previous = m.length;
    auto seq = m.sequence; now += 600; m.update(now, {}, random);
    CHECK(m.length == previous + 1);
    for (unsigned n = 0; n < previous; ++n) CHECK(m.sequence[n] == seq[n]);
    now += m.length * 600; m.update(now, {}, random); m.update(++now, {}, random);
    CHECK(m.phase == Memory::Phase::Answer);
  }
  CHECK(m.perfect && m.score == 12); CHECK(m.sequence[0] == prefix[0]); CHECK(m.update(++now, {}, random));
  m.enter(0, random); m.update(1200, {}, random); m.update(1201, {}, random);
  CHECK(m.update(1300, m.sequence[0] ? shortA() : shortB(), random)); CHECK(m.score == 0);
  m.enter(UINT32_MAX - 1000, random); m.update(m.phaseAt + 1200, {}, random); m.update(m.phaseAt + 1201, {}, random);
  const auto at = m.answerAt; CHECK(!m.update(at + 3000, {}, random)); CHECK(m.update(at + 3021, {}, random));
  m.enter(0, random); m.update(1200, {}, random); m.update(1201, {}, random);
  auto last = m.sequence[0] ? shortB() : shortA();
  auto& event = m.sequence[0] ? last.b : last.a;
  event.timestamped = true; event.releasedAt = 4201;
  CHECK(!m.update(4221, last, random)); CHECK(m.cursor == 1);
}
static void testClock() {
  Clock clock;
  CHECK(!clock.update(20, pressA(0))); CHECK(clock.phase == Clock::Phase::Running);
  clock.update(100, pressA(0)); CHECK(clock.phase == Clock::Phase::Running);
  clock.update(500, {}); CHECK(clock.released);
  CHECK(clock.update(10020, pressA(10000))); CHECK(clock.error() == 0);
}
static void testClockValues() {
  for (uint32_t elapsed : {0u, 9999u, 10000u, 10001u, 19999u, 20000u}) {
    Clock clock; const uint32_t start = UINT32_MAX - 4000;
    clock.update(start + 20, pressA(start)); clock.update(start + 30, {});
    CHECK(clock.update(start + elapsed + 20, pressA(start + elapsed)));
    CHECK(clock.timeout == (elapsed >= 20000)); CHECK(clock.elapsedMs == elapsed);
    CHECK(clock.error() == (elapsed > 10000 ? elapsed - 10000 : 10000 - elapsed));
  }
  Clock clock; clock.update(20, pressA(0)); clock.update(50, {}); CHECK(clock.update(20020, {})); CHECK(clock.timeout);
  Clock last; Input raw;
  last.update(20, pressA(0)); last.update(50, {}); raw.update(true, false, 19999);
  for (uint32_t now = 20000; now <= 20019; ++now) last.update(now, raw.update(true, false, now));
  CHECK(!last.timeout && last.elapsedMs == 19999);
}
static void testFlight() {
  Random random(1); Flight f; f.enter(0);
  CHECK(!f.update(1999, .45f, random)); CHECK(!f.obstacles[0].active);
  CHECK(!f.update(2000, .45f, random)); CHECK(f.obstacles[0].active);
  CHECK(f.y == (cfg::fieldTop + cfg::fieldBottom) * .5f);
  Obstacle obstacle{40, f.y, true}; CHECK(!Flight::hits(f.y, obstacle)); CHECK(Flight::hits(float(cfg::fieldTop), obstacle));
  obstacle.active = false; CHECK(!Flight::hits(0, obstacle)); obstacle = {100, f.y, true}; CHECK(!Flight::hits(0, obstacle));
  f.enter(0); f.update(1000, 1, random); CHECK(f.y == cfg::fieldTop + 10);
  f.update(2000, 0, random); CHECK(f.y <= cfg::fieldBottom - 10);
  f.enter(0); f.y = cfg::fieldTop + 10; f.obstacles[0] = {100, 75, true}; CHECK(f.update(2000, .45f, random)); CHECK(f.collision && f.elapsedMs < 2000);
  f.enter(UINT32_MAX - 100); CHECK(f.update(f.startedAt + 20000, .45f, random)); CHECK(!f.collision && f.elapsedMs == 20000);
  float previous = f.lastCenter;
  for (unsigned n = 0; n < 100; ++n) {
    for (auto& o : f.obstacles) o.active = false;
    f.spawn(random); CHECK(std::abs(f.lastCenter - previous) <= 25); previous = f.lastCenter;
    CHECK(f.lastCenter >= cfg::fieldTop + 35 && f.lastCenter <= cfg::fieldBottom - 35);
  }
}
static void testMicAndRecords() {
  const int16_t dc[4] = {2000, 2000, 2000, 2000}; CHECK(rmsWithoutDC(dc, 4) == 0);
  const int16_t ac[4] = {900, 1100, 900, 1100}; CHECK(std::abs(rmsWithoutDC(ac, 4) - 100) < .001f);
  CHECK(rmsWithoutDC(nullptr, 0) == 0);
  Calibration cal; for (unsigned n = 0; n < 20; ++n) cal.sample(100); cal.beginVoice();
  for (unsigned n = 0; n < 20; ++n) cal.sample(110); CHECK(!cal.finish()); CHECK(cal.normalize(100) == 0);
  cal.reset(); for (unsigned n = 0; n < 20; ++n) cal.sample(100); cal.beginVoice();
  for (unsigned n = 0; n < 20; ++n) cal.sample(200); CHECK(cal.finish());
  CHECK(std::abs(cal.normalize(200) - .2f) < .0001f);
  for (unsigned n = 0; n < 100; ++n) cal.normalize(300); CHECK(cal.filtered <= 1);
  CHECK(cal.normalize(std::numeric_limits<float>::quiet_NaN()) == 0);
  Records r; CHECK(r.valid()); CHECK(r.submit(Game::Clock, Mode::Voice, 0)); CHECK(r.best[5].valid);
  CHECK(!r.submit(Game::Clock, Mode::Voice, 1)); CHECK(!r.submit(Game::Clock, Mode::Voice, 0));
  CHECK(r.submit(Game::Flight, Mode::Voice, 100)); CHECK(r.submit(Game::Flight, Mode::Button, 20));
  CHECK(r.best[3].value == 100 && r.best[4].value == 20);
  auto bytes = encode(r); Records roundTrip; CHECK(decode(bytes, roundTrip)); CHECK(roundTrip.best[5].valid && roundTrip.best[5].value == 0);
  for (unsigned n = 0; n < bytes.size(); ++n) { auto corrupted = bytes; corrupted[n] ^= 1; CHECK(!decode(corrupted, roundTrip)); CHECK(!roundTrip.best[5].valid); }
  auto invalid = r; invalid.settings.brightness = 3; CHECK(!decode(encode(invalid), roundTrip));
  invalid = r; invalid.best[0] = {true, 1001}; CHECK(!decode(encode(invalid), roundTrip));
  r.settings.sound = false; r.reset(); CHECK(!r.best[3].valid && !r.settings.sound);
}
static void select(App& app, Game game, Mode mode, uint32_t now = 1) {
  app.records.settings.mode = mode; app.selection = unsigned(game); app.update(now, {});
  app.update(now + 1, shortB()); CHECK(app.screen == Screen::Instructions);
  app.update(now + 2, {}); app.update(now + 3, shortA());
}
static void testApp() {
  for (Game game : {Game::Baseball, Game::Balloon, Game::Memory, Game::Flight, Game::Clock}) {
    App app(1); select(app, game, Mode::Button);
    CHECK(app.screen == Screen::Countdown); app.update(1504, {}); CHECK(app.screen == Screen::Play);
    app.update(1505, {});
    uint32_t endedAt = 22000;
    if (game == Game::Baseball) {
      uint32_t now = 1506;
      for (unsigned n = 0; n < 10; ++n) { now = app.baseball.pitchAt + app.baseball.waitMs + app.baseball.travelMs; app.update(now + 20, pressA(now)); app.update(now + 620, {}); }
      endedAt = now + 620;
    } else if (game == Game::Balloon) app.update(1600, shortB());
    else if (game == Game::Memory) { app.update(2704, {}); app.update(2705, {}); app.update(2800, app.memory.sequence[0] ? shortA() : shortB()); }
    else if (game == Game::Flight) app.update(22000, {});
    else { app.update(1600, pressA(1580)); app.update(1700, {}); app.update(11600, pressA(11580)); }
    CHECK(app.screen == Screen::Result); CHECK(app.takeStorageDirty()); CHECK(!app.takeStorageDirty());
    app.update(endedAt + 1, {}); app.update(endedAt + 2, shortA()); CHECK(app.screen == Screen::Countdown);
    app.update(endedAt + 1502, {}); CHECK(app.screen == Screen::Play);
    InputFrame abort; abort.abort = true; app.update(endedAt + 1503, abort); CHECK(app.screen == Screen::Menu); CHECK(!app.takeStorageDirty());
  }
  App app; select(app, Game::Clock, Mode::Button);
  auto hold = pressA(1500); app.update(1504, hold); CHECK(app.screen == Screen::Play);
  app.update(1505, hold); CHECK(app.clock.phase == Clock::Phase::Ready); app.update(1510, {});
  app.update(1600, pressA(1580)); app.update(21600, {}); CHECK(app.result.timeout); CHECK(!app.records.best[5].valid);
  app.update(52000, {}); CHECK(app.screen == Screen::Menu);
  App settings; settings.update(1, {}); settings.selection = 5; settings.update(2, shortB()); settings.update(3, {});
  settings.update(4, shortB()); CHECK(!settings.records.settings.sound && settings.takeStorageDirty());
  settings.setting = 4; settings.records.submit(Game::Clock, Mode::Voice, 0); settings.update(5, shortB()); settings.update(6, {});
  InputFrame b; b.b.down = true; b.b.heldMs = 1999; settings.update(2000, b); CHECK(settings.screen == Screen::ResetConfirm);
  b.b.heldMs = 2000; settings.update(2001, b); CHECK(settings.screen == Screen::Settings && !settings.records.best[5].valid);
  CHECK(!settings.records.settings.sound && settings.takeStorageDirty());
  App voice; select(voice, Game::Flight, Mode::Voice); CHECK(voice.screen == Screen::CalQuiet && voice.wantsMic());
  for (uint32_t at = 20; at <= 2004; at += 20) voice.update(at, {}, {true, true, 100});
  voice.update(2004, {}, {true, true, 100}); CHECK(voice.screen == Screen::CalVoice);
  for (uint32_t at = 2024; at <= 4004; at += 20) voice.update(at, {}, {true, true, 200});
  CHECK(voice.screen == Screen::Countdown && voice.calibration.valid && voice.muted());
  voice.update(5504, {}, {true, true, 100}); CHECK(voice.screen == Screen::Play);
  voice.update(5505, {}, {false, false, 0}); CHECK(voice.screen == Screen::MicError && !voice.wantsMic());
  CHECK(!voice.records.best[3].valid); voice.update(5506, {}); voice.update(5507, shortB()); CHECK(voice.mode == Mode::Button && voice.screen == Screen::Countdown);
  voice.update(7007, {}); voice.update(7008, {}); voice.update(28000, {}); CHECK(voice.records.best[4].valid && !voice.records.best[3].valid);
  App stale; select(stale, Game::Flight, Mode::Voice); stale.update(504, {}); CHECK(stale.screen == Screen::MicError);
  App gap; select(gap, Game::Flight, Mode::Voice);
  for (uint32_t at = 24; at <= 4004; at += 20) gap.update(at, {}, {true, true, 100});
  CHECK(gap.screen == Screen::MicError);
}
static void testPhysicalTransitions() {
  App app; select(app, Game::Balloon, Mode::Button); app.update(1504, {}); app.update(1505, {});
  Input physical;
  auto tick = [&](uint32_t now, bool a, bool b) { app.update(now, physical.update(a, b, now)); };
  tick(1510, true, true); tick(1530, true, true);
  tick(1600, false, true); tick(1620, false, true);
  tick(1700, false, false); tick(1720, false, false);
  CHECK(app.screen == Screen::Play && app.balloon.stage == 0);
  tick(1800, true, false); tick(1820, true, false);
  tick(1850, false, false); tick(1870, false, false); CHECK(app.balloon.stage == 1);
  tick(2000, true, true); tick(2020, true, true); tick(2820, false, false);
  CHECK(app.screen == Screen::Menu); tick(2840, false, false);
  CHECK(app.selection == 1 && !app.records.best[1].valid && !app.takeStorageDirty());
  App reset; reset.records.submit(Game::Clock, Mode::Voice, 0); reset.selection = 5;
  reset.update(1, {}); reset.update(2, shortB()); reset.update(3, {}); reset.setting = 4;
  reset.update(4, shortB()); reset.update(5, {});
  reset.update(6, shortB()); CHECK(reset.screen == Screen::ResetConfirm && reset.records.best[5].valid);
  reset.update(7, shortA()); CHECK(reset.screen == Screen::Settings && reset.records.best[5].valid && !reset.takeStorageDirty());
  App sound; sound.game = Game::Flight; sound.mode = Mode::Voice; sound.screen = Screen::Play;
  CHECK(sound.wantsMic() && sound.muted()); sound.screen = Screen::Result;
  CHECK(!sound.wantsMic() && !sound.muted());
}
int main() {
  testInput(); testBaseball(); testBalloon(); testMemory(); testClock(); testClockValues(); testFlight(); testMicAndRecords(); testApp(); testPhysicalTransitions();
  std::printf("PASS: %u assertions (input, 5 games, calibration, NVS codec, app transitions)\n", assertions);
}
