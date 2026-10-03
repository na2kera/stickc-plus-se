#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <cmath>
#include "input.hpp"
namespace mini {
enum class Game : uint8_t { Baseball, Balloon, Memory, Flight, Clock };
enum class Mode : uint8_t { Voice, Button };
enum class Screen : uint8_t {
  Menu, Instructions, Countdown, Play, Result, Settings, ResetConfirm,
  CalQuiet, CalVoice, CalDone, MicError
};
enum class Sound : uint8_t { None, Start, Success, Failure, Best };
struct Settings { bool sound = true; uint8_t brightness = 1; Mode mode = Mode::Voice; };
struct Best { bool valid = false; uint32_t value = 0; };
struct Records {
  std::array<Best, 6> best{};
  Settings settings;
  bool valid() const;
  unsigned index(Game game, Mode mode) const {
    if (game == Game::Clock) return 5;
    if (game == Game::Flight) return mode == Mode::Voice ? 3 : 4;
    return static_cast<unsigned>(game);
  }
  bool submit(Game game, Mode mode, uint32_t value);
  void reset() { best = {}; }
};
constexpr size_t storageSize = 44;
using StorageBytes = std::array<uint8_t, storageSize>;
StorageBytes encode(const Records& records);
bool decode(const StorageBytes& bytes, Records& records);
class Random {
  uint32_t state_;
public:
  explicit Random(uint32_t seed = 1) : state_(seed ? seed : 1) {}
  uint32_t next() {
    state_ ^= state_ << 13; state_ ^= state_ >> 17; state_ ^= state_ << 5; return state_;
  }
  uint32_t range(uint32_t low, uint32_t high) {
    const uint32_t width = high - low + 1, threshold = uint32_t(-width) % width;
    uint32_t v; do { v = next(); } while (v < threshold);
    return low + v % width;
  }
};
struct MicFrame { bool fresh = false, ok = true; float rms = 0; };
float rmsWithoutDC(const int16_t* samples, size_t count);
struct Calibration {
  float quiet = 0, voice = 0, filtered = 0;
  double sum = 0; unsigned count = 0;
  bool valid = false;
  void reset() { *this = {}; }
  void sample(float rms) { if (std::isfinite(rms) && rms >= 0) { sum += rms; ++count; } }
  void beginVoice() { quiet = count ? float(sum / count) : 0; sum = 0; count = 0; }
  bool finish() {
    voice = count ? float(sum / count) : 0;
    valid = count >= 20 && voice - quiet >= cfg::minCalibrationGap;
    filtered = 0; return valid;
  }
  float normalize(float rms) {
    if (!valid || !std::isfinite(rms)) return 0;
    float level = (rms - quiet) / (voice - quiet);
    level = level < 0 ? 0 : (level > 1 ? 1 : level);
    filtered += cfg::alpha * (level - filtered); return filtered;
  }
};
struct Result {
  uint32_t score = 0, elapsedMs = 0;
  bool eligible = true, timeout = false, perfect = false, burst = false, newBest = false;
  Mode mode = Mode::Voice;
};
struct Baseball {
  uint32_t pitchAt = 0, waitMs = 0, travelMs = 0, feedbackAt = 0;
  unsigned ball = 1, score = 0, lastPoints = 0;
  bool feedback = false;
  void pitch(uint32_t now, Random& random);
  static unsigned points(int32_t delta);
  bool update(uint32_t now, const InputFrame& input, Random& random);
};
struct Balloon {
  unsigned stage = 0, burstAt = 12;
  uint32_t startedAt = 0;
  bool burst = false;
  void enter(uint32_t now, Random& random, unsigned fixed = 0);
  bool update(uint32_t now, const InputFrame& input);
};
struct Memory {
  std::array<uint8_t, 12> sequence{};
  unsigned length = 2, cursor = 0, score = 0;
  enum class Phase { Show, Release, Answer, Success, Done } phase = Phase::Show;
  uint32_t phaseAt = 0, answerAt = 0;
  bool perfect = false;
  void enter(uint32_t now, Random& random);
  bool update(uint32_t now, const InputFrame& input, Random& random);
  int symbol(uint32_t now) const;
};
struct Clock {
  enum class Phase { Ready, Running, Done } phase = Phase::Ready;
  uint32_t startedAt = 0, elapsedMs = 0;
  bool released = false, timeout = false;
  bool update(uint32_t now, const InputFrame& input);
  uint32_t error() const { return elapsedMs > 10000 ? elapsedMs - 10000 : 10000 - elapsedMs; }
};
struct Obstacle { float x = 0, center = 0; bool active = false; float gap = cfg::gapHeight; };
struct Flight {
  std::array<Obstacle, 4> obstacles{};
  float y = (cfg::fieldTop + cfg::fieldBottom) * .5f, lastCenter = y, level = 0;
  uint32_t startedAt = 0, updatedAt = 0, nextSpawn = cfg::obstacleIntervalMs, elapsedMs = 0;
  bool collision = false;
  bool nextHigh = false;
  unsigned difficulty() const;
  float speed() const;
  float gap() const;
  uint32_t spawnInterval() const;
  void enter(uint32_t now);
  void spawn(Random& random);
  static bool hits(float y, const Obstacle& obstacle);
  bool update(uint32_t now, float inputLevel, Random& random);
};
class App {
  uint32_t enteredAt_ = 0, lastActivity_ = 0, lastMicAt_ = 0;
  bool gate_ = true, storageDirty_ = false, calibrationOnly_ = false;
  Sound sound_ = Sound::None;
  void transition(Screen next, uint32_t now);
  void start(uint32_t now);
  void finish(uint32_t now);
  void beginCalibration(uint32_t now);
public:
  Screen screen = Screen::Menu;
  Game game = Game::Baseball;
  Mode mode = Mode::Voice;
  unsigned selection = 0, setting = 0;
  Records records;
  Result result;
  Random random;
  Calibration calibration;
  Baseball baseball;
  Balloon balloon;
  Memory memory;
  Clock clock;
  Flight flight;
  explicit App(uint32_t seed = 1) : random(seed) {}
  void update(uint32_t now, InputFrame input, const MicFrame& mic = {});
  uint32_t screenElapsed(uint32_t now) const { return now - enteredAt_; }
  bool wantsMic() const;
  bool muted() const { return wantsMic(); }
  bool takeStorageDirty() { bool value = storageDirty_; storageDirty_ = false; return value; }
  Sound takeSound() { auto value = sound_; sound_ = Sound::None; return value; }
};
}
