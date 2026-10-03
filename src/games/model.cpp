#include "model.hpp"
#include <algorithm>
namespace mini {
bool Records::valid() const {
  if (settings.brightness > 2 || uint8_t(settings.mode) > 1) return false;
  constexpr uint32_t maximum[6] = {1000, 230, 12, UINT32_MAX / 100, UINT32_MAX / 100, 10000};
  for (size_t i = 0; i < best.size(); ++i) {
    if (best[i].value > maximum[i] || (!best[i].valid && best[i].value)) return false;
  }
  return true;
}
bool Records::submit(Game game, Mode mode, uint32_t value) {
  auto& target = best[index(game, mode)];
  if (!target.valid || (game == Game::Clock ? value < target.value : value > target.value)) {
    target = {true, value}; return true;
  }
  return false;
}
static void put(StorageBytes& b, size_t i, uint32_t value) {
  for (unsigned j = 0; j < 4; ++j) b[i + j] = uint8_t(value >> (j * 8));
}
static uint32_t get(const StorageBytes& b, size_t i) {
  uint32_t value = 0; for (unsigned j = 0; j < 4; ++j) value |= uint32_t(b[i + j]) << (j * 8); return value;
}
static uint32_t checksum(const StorageBytes& b) {
  uint32_t value = 2166136261u; for (size_t i = 0; i < 40; ++i) { value ^= b[i]; value *= 16777619u; } return value;
}
StorageBytes encode(const Records& r) {
  StorageBytes b{}; put(b, 0, 0x53474d35); put(b, 4, 1); // magic, schemaVersion
  b[8] = r.settings.sound; b[9] = r.settings.brightness; b[10] = uint8_t(r.settings.mode);
  for (unsigned i = 0; i < 6; ++i) { if (r.best[i].valid) b[11] |= 1 << i; put(b, 12 + i * 4, r.best[i].value); }
  put(b, 40, checksum(b)); return b;
}
bool decode(const StorageBytes& b, Records& r) {
  r = {};
  if (get(b, 0) != 0x53474d35 || get(b, 4) != 1 || get(b, 40) != checksum(b) || b[8] > 1 || b[11] > 63 || get(b, 36)) return false;
  Records candidate;
  candidate.settings = {bool(b[8]), b[9], Mode(b[10])};
  for (unsigned i = 0; i < 6; ++i) candidate.best[i] = {bool(b[11] & (1 << i)), get(b, 12 + i * 4)};
  if (!candidate.valid()) return false;
  r = candidate; return true;
}
float rmsWithoutDC(const int16_t* samples, size_t count) {
  if (!samples || !count) return 0;
  double sum = 0, square = 0;
  for (size_t i = 0; i < count; ++i) { sum += samples[i]; square += double(samples[i]) * samples[i]; }
  const double mean = sum / count;
  return float(std::sqrt(std::max(0., square / count - mean * mean)));
}
void Baseball::pitch(uint32_t now, Random& random) {
  pitchAt = now; waitMs = random.range(500, 900); travelMs = random.range(700, 1100); feedback = false;
}
unsigned Baseball::points(int32_t delta) {
  const uint32_t error = delta < 0 ? uint32_t(-int64_t(delta)) : uint32_t(delta);
  return error <= 50 ? 100 : (error <= 100 ? 50 : (error <= 150 ? 20 : 0));
}
bool Baseball::update(uint32_t now, const InputFrame& input, Random& random) {
  if (feedback) {
    if (uint32_t(now - feedbackAt) >= cfg::pitchFeedbackMs) {
      if (ball == 10) return true;
      ++ball; pitch(now, random);
    }
    return false;
  }
  const uint32_t arrival = pitchAt + waitMs + travelMs;
  // Keep the late boundary open until the common debounce can deliver a press
  // whose timestamp is exactly arrival+150ms.
  if (input.a.pressed || int32_t(now - arrival) > int32_t(150 + cfg::debounceMs)) {
    lastPoints = input.a.pressed ? points(int32_t(input.a.pressedAt - arrival)) : 0;
    score += lastPoints; feedback = true; feedbackAt = now;
  }
  return false;
}
void Balloon::enter(uint32_t now, Random& random, unsigned fixed) {
  *this = {}; startedAt = now; burstAt = fixed >= 12 && fixed <= 24 ? fixed : random.range(12, 24);
}
bool Balloon::update(uint32_t now, const InputFrame& input) {
  const uint32_t aAt = input.a.timestamped ? input.a.releasedAt : now;
  const uint32_t bAt = input.b.timestamped ? input.b.releasedAt : now;
  const bool a = input.a.shortPress && uint32_t(aAt - startedAt) < cfg::balloonMs;
  const bool b = input.b.shortPress && uint32_t(bAt - startedAt) < cfg::balloonMs;
  if (a) { ++stage; if (stage >= burstAt) { burst = true; return true; } }
  return b || uint32_t(now - startedAt) >= cfg::balloonMs + cfg::debounceMs;
}
void Memory::enter(uint32_t now, Random& random) {
  *this = {}; phaseAt = now; sequence[0] = random.range(0, 1); sequence[1] = random.range(0, 1);
}
int Memory::symbol(uint32_t now) const {
  if (phase != Phase::Show) return -1;
  const uint32_t elapsed = now - phaseAt, span = cfg::memoryShowMs + cfg::memoryBlankMs;
  if (elapsed / span >= length || elapsed % span >= cfg::memoryShowMs) return -1;
  return sequence[elapsed / span];
}
bool Memory::update(uint32_t now, const InputFrame& input, Random& random) {
  switch (phase) {
    case Phase::Show:
      if (uint32_t(now - phaseAt) >= length * (cfg::memoryShowMs + cfg::memoryBlankMs)) phase = Phase::Release;
      break;
    case Phase::Release:
      if (!input.a.down && !input.b.down) { phase = Phase::Answer; answerAt = now; cursor = 0; }
      break;
    case Phase::Answer:
      if (input.a.shortPress || input.b.shortPress) {
        const auto& event = input.a.shortPress ? input.a : input.b;
        const uint32_t at = event.timestamped ? event.releasedAt : now;
        if (uint32_t(at - answerAt) > cfg::memoryInputMs) { phase = Phase::Done; return true; }
        if ((input.a.shortPress && input.b.shortPress) || sequence[cursor] != (input.b.shortPress ? 1 : 0)) {
          phase = Phase::Done; return true;
        }
        ++cursor; answerAt = at;
        if (cursor == length) {
          score = length; perfect = length == sequence.size();
          if (perfect) { phase = Phase::Done; return true; }
          phase = Phase::Success; phaseAt = now;
        }
      }
      else if (uint32_t(now - answerAt) > cfg::memoryInputMs + cfg::debounceMs) { phase = Phase::Done; return true; }
      break;
    case Phase::Success:
      if (uint32_t(now - phaseAt) >= 600) {
        sequence[length++] = random.range(0, 1); phase = Phase::Show; phaseAt = now;
      }
      break;
    case Phase::Done: return true;
  }
  return false;
}
bool Clock::update(uint32_t now, const InputFrame& input) {
  if (phase == Phase::Ready) {
    if (input.a.pressed) { startedAt = input.a.pressedAt; phase = Phase::Running; released = false; }
    return false;
  }
  if (phase == Phase::Done) return true;
  if (!input.a.down) released = true;
  if (released && input.a.pressed && uint32_t(input.a.pressedAt - startedAt) < 20000) {
    elapsedMs = input.a.pressedAt - startedAt; phase = Phase::Done; return true;
  }
  elapsedMs = now - startedAt;
  if (elapsedMs >= 20000 + cfg::debounceMs) { elapsedMs = 20000; timeout = true; phase = Phase::Done; return true; }
  return false;
}
void Flight::enter(uint32_t now) { *this = {}; startedAt = updatedAt = now; }
unsigned Flight::difficulty() const { return std::min(unsigned(elapsedMs / cfg::flightDifficultyMs), cfg::flightMaxStage); }
float Flight::speed() const { return cfg::obstacleSpeed + 10.f * difficulty(); }
float Flight::gap() const { return cfg::gapHeight - 3.f * difficulty(); }
uint32_t Flight::spawnInterval() const { return cfg::obstacleIntervalMs - 100 * difficulty(); }
void Flight::spawn(Random& random) {
  const float opening = gap();
  const float low = cfg::fieldTop + opening * .5f, high = cfg::fieldBottom - opening * .5f;
  // Alternate upper/lower routes, with jitter and a reachable change in height.
  const float target = nextHigh ? high - float(random.range(0, 3)) : low + float(random.range(0, 3));
  const float center = std::max(low, std::min(high, std::max(lastCenter - cfg::flightCenterStep, std::min(lastCenter + cfg::flightCenterStep, target))));
  for (auto& obstacle : obstacles) if (!obstacle.active) {
    obstacle = {float(cfg::width), center, true, opening};
    lastCenter = center; nextHigh = !nextHigh; break;
  }
}
bool Flight::hits(float y, const Obstacle& o) {
  return o.active && o.x < cfg::flightX + cfg::flightRadius && o.x + cfg::obstacleWidth > cfg::flightX - cfg::flightRadius
    && (y - cfg::flightRadius < o.center - o.gap * .5f || y + cfg::flightRadius > o.center + o.gap * .5f);
}
bool Flight::update(uint32_t now, float inputLevel, Random& random) {
  level = std::max(0.f, std::min(1.f, inputLevel));
  if (collision) return true;
  // Integrate in <=10ms steps: collision cannot tunnel through a bar on a slow frame.
  uint32_t remaining = uint32_t(now - updatedAt);
  while (remaining) {
    const uint32_t step = std::min(remaining, uint32_t(10)); remaining -= step;
    elapsedMs += step; updatedAt += step;
    y = std::max(cfg::fieldTop + 10.f, std::min(cfg::fieldBottom - 10.f, y + (45.f - 100.f * level) * (step * .001f)));
    if (elapsedMs >= nextSpawn) { spawn(random); nextSpawn += spawnInterval(); }
    for (auto& o : obstacles) if (o.active) {
      o.x -= speed() * (step * .001f);
      if (hits(y, o)) { collision = true; return true; }
      if (o.x + cfg::obstacleWidth < 0) o.active = false;
    }
  }
  return false;
}
}
