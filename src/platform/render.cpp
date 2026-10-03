#include "render.hpp"
#include <algorithm>
#include <cstdio>
#ifdef MINIGAMES_RENDER_QA
#include <stdexcept>
#endif
namespace mini {
namespace {
constexpr uint16_t bg = 0x0843, panel = 0x1927, white = 0xffff, muted = 0xad75;
constexpr uint16_t yellow = 0xff25, mint = 0x6ff3, red = 0xfb0c, sky = 0x5dff;
const char* names[5] = {"ホームラン競争", "チキン風船", "記憶スイッチ", "声で浮く気球", "ぴったり10秒"};
void text(lgfx::LGFXBase& c, const char* value, int x, int y, uint16_t color = white, bool large = false) {
  c.setFont(large ? &fonts::lgfxJapanGothic_24 : &fonts::lgfxJapanGothic_16);
#ifdef MINIGAMES_RENDER_QA
  if (x < 0 || x + c.textWidth(value) > cfg::width || y < 0 || y + c.fontHeight() > cfg::height)
    throw std::runtime_error(std::string("Text overflow: ") + value);
#endif
  c.setTextColor(color); c.setTextDatum(lgfx::textdatum_t::top_left); c.drawString(value, x, y);
}
void number(lgfx::LGFXBase& c, const char* value, int x, int y, uint16_t color = yellow, float scale = 1) {
  c.setFont(&fonts::Font4); c.setTextColor(color); c.setTextSize(scale); c.drawString(value, x, y); c.setTextSize(1);
}
void footer(lgfx::LGFXBase& c, const char* value) {
  c.fillRect(0, 115, 240, 20, panel); text(c, value, 6, 117, muted);
}
void balloonIcon(lgfx::LGFXBase& c, int x, int y, int radius, uint16_t color, bool worried = false) {
  c.fillEllipse(x, y, radius, radius + 3, color);
  c.fillCircle(x - radius / 3, y - 3, 2, bg); c.fillCircle(x + radius / 3, y - 3, 2, bg);
  if (worried) { c.drawLine(x - 4, y + 6, x, y + 3, bg); c.drawLine(x, y + 3, x + 4, y + 6, bg); }
  else c.drawLine(x - 4, y + 5, x + 4, y + 5, bg);
  c.drawLine(x, y + radius + 3, x - 3, y + radius + 15, white);
}
void clockIcon(lgfx::LGFXBase& c, int x, int y, int radius) {
  c.drawCircle(x, y, radius, white); c.drawCircle(x, y, radius - 1, white);
  c.drawLine(x, y, x, y - radius / 2, white); c.drawLine(x, y, x + radius / 2, y, white);
}
void icon(lgfx::LGFXBase& c, Game game, int x, int y) {
  switch (game) {
    case Game::Baseball:
      c.fillCircle(x + 12, y - 5, 17, white); c.drawLine(x + 5, y - 17, x + 17, y + 6, red);
      c.fillRoundRect(x - 32, y - 26, 10, 50, 4, yellow); break;
    case Game::Balloon: balloonIcon(c, x, y - 7, 25, mint); break;
    case Game::Memory:
      c.fillCircle(x - 25, y, 20, yellow); c.fillRoundRect(x + 4, y - 20, 40, 40, 4, sky);
      number(c, "A", x - 34, y - 12, bg); number(c, "B", x + 15, y - 12, bg); break;
    case Game::Flight:
      balloonIcon(c, x, y - 12, 20, sky); c.fillRect(x - 8, y + 18, 16, 10, yellow); break;
    case Game::Clock: clockIcon(c, x, y, 28); break;
  }
}
void bestLabel(lgfx::LGFXBase& c, const App& app, int y) {
  char buffer[50]; const Best& best = app.records.best[app.records.index(app.game, app.mode)];
  if (!best.valid) std::snprintf(buffer, sizeof(buffer), "ベスト --");
  else if (app.game == Game::Clock) std::snprintf(buffer, sizeof(buffer), "ベスト誤差 %lu ms", static_cast<unsigned long>(best.value));
  else if (app.game == Game::Flight) std::snprintf(buffer, sizeof(buffer), "ベスト %lu.%lu 秒", static_cast<unsigned long>(best.value / 10), static_cast<unsigned long>(best.value % 10));
  else std::snprintf(buffer, sizeof(buffer), "ベスト %lu", static_cast<unsigned long>(best.value));
  text(c, buffer, 8, y, mint);
}
void gameRender(lgfx::LGFXBase& c, const App& app, uint32_t now) {
  char buffer[64];
  footer(c, "A+B 長押しで中断");
  switch (app.game) {
    case Game::Baseball: {
      const auto& b = app.baseball;
      std::snprintf(buffer, sizeof(buffer), "%u/10球 %u点 Lv%u", b.ball, b.score, b.difficulty() + 1); text(c, buffer, 8, 4);
      c.fillCircle(24, 59, 8, sky); c.fillRect(20, 68, 8, 24, sky);
      c.drawLine(24, 87, 12, 107, sky); c.drawLine(24, 87, 34, 107, sky);
      c.fillRoundRect(45, 61, 7, 40, 3, yellow); c.drawLine(51, 36, 51, 110, muted);
      c.drawLine(6, 108, 235, 108, muted);
      if (b.feedback) {
        const char* hit = b.lastPoints == 100 ? "ホームラン!" : b.lastPoints == 50 ? "ヒット!" : b.lastPoints == 20 ? "かすり" : "空振り";
        text(c, hit, 83, 48, b.lastPoints ? yellow : red, true);
        std::snprintf(buffer, sizeof(buffer), "+%u", b.lastPoints); number(c, buffer, 130, 82);
      } else {
        const int32_t elapsed = int32_t(now - b.pitchAt) - int32_t(b.waitMs);
        if (elapsed >= 0) {
          const float t = std::min(1.15f, float(elapsed) / b.travelMs);
          c.fillCircle(int(228 - 177 * t), 80, 6, white);
        }
        text(c, "Aで打つ", 102, 29, muted);
      }
      break;
    }
    case Game::Balloon: {
      const auto& b = app.balloon;
      const uint32_t left = cfg::balloonMs - std::min(cfg::balloonMs, uint32_t(now - b.startedAt));
      std::snprintf(buffer, sizeof(buffer), "残り %lu秒", static_cast<unsigned long>((left + 999) / 1000)); text(c, buffer, 8, 4);
      std::snprintf(buffer, sizeof(buffer), "%u", b.stage * 10); number(c, buffer, 165, 50);
      const float ratio = float(b.stage) / b.burstAt;
      const int shake = ratio >= .7f ? int((now / 80) % 3) - 1 : 0;
      balloonIcon(c, 86 + shake * 2, 64, std::min(30, 10 + int(b.stage)), ratio >= .9f ? red : mint, ratio >= .7f);
      footer(c, "A:ふくらます B:確定"); break;
    }
    case Game::Memory: {
      const auto& m = app.memory;
      std::snprintf(buffer, sizeof(buffer), "長さ %u  ベスト %u", m.length, m.score); text(c, buffer, 8, 4);
      const int symbol = m.symbol(now);
      if (symbol == 0) { c.fillCircle(70, 70, 29, yellow); number(c, "A", 60, 56, bg); }
      if (symbol == 1) { c.fillRoundRect(140, 40, 60, 60, 6, sky); number(c, "B", 159, 56, bg); }
      if (m.phase == Memory::Phase::Answer) {
        text(c, "順番を入力", 56, 37, white, true);
        std::snprintf(buffer, sizeof(buffer), "%u / %u", m.cursor, m.length); number(c, buffer, 83, 72);
        footer(c, "A / B を短く押す");
      } else if (m.phase == Memory::Phase::Release) text(c, "ボタンを離して", 40, 60);
      else if (m.phase == Memory::Phase::Success) text(c, "正解!", 80, 52, mint, true);
      else footer(c, "順番を覚えよう");
      break;
    }
    case Game::Flight: {
      const auto& f = app.flight;
      std::snprintf(buffer, sizeof(buffer), "%lu.%lu秒", static_cast<unsigned long>(f.elapsedMs / 1000), static_cast<unsigned long>(f.elapsedMs / 100 % 10)); text(c, buffer, 6, 4);
      c.drawRect(170, 7, 62, 10, muted); c.fillRect(172, 9, int(f.level * 58), 6, mint);
      for (const auto& o : f.obstacles) if (o.active) {
        int x = int(o.x), upper = int(o.center - o.gap / 2), lower = int(o.center + o.gap / 2);
        c.fillRect(x, cfg::fieldTop, int(cfg::obstacleWidth), upper - cfg::fieldTop, mint);
        c.fillRect(x, lower, int(cfg::obstacleWidth), cfg::fieldBottom - lower, mint);
      }
      c.drawLine(0, cfg::fieldTop, 239, cfg::fieldTop, muted); c.drawLine(0, cfg::fieldBottom, 239, cfg::fieldBottom, muted);
      c.fillEllipse(int(cfg::flightX), int(f.y - 2), 9, 8, sky); c.fillRect(int(cfg::flightX) - 4, int(f.y) + 6, 8, 4, yellow);
      std::snprintf(buffer, sizeof(buffer), "Lv%u  A+B 長押しで中断", f.difficulty() + 1); footer(c, buffer);
      break;
    }
    case Game::Clock: {
      const auto& clock = app.clock;
      text(c, "ぴったり10秒", 8, 4);
      if (clock.phase == Clock::Phase::Ready) {
        number(c, "READY", 73, 40, mint); clockIcon(c, 39, 73, 23); footer(c, "Aで計測開始");
      } else {
        if (clock.elapsedMs < 2000) {
          std::snprintf(buffer, sizeof(buffer), "%lu.%02lu", static_cast<unsigned long>(clock.elapsedMs / 1000), static_cast<unsigned long>(clock.elapsedMs % 1000 / 10)); number(c, buffer, 89, 52, yellow, 1.5f);
        } else { number(c, "?", 116, 47, yellow, 2); clockIcon(c, 45, 72, 23); }
        footer(c, "次のA押下で止める");
      }
      break;
    }
  }
}
}
void drawScreen(lgfx::LGFXBase& c, const App& app, uint32_t now, bool boardReady, bool storageError) {
  c.fillScreen(bg); c.setTextSize(1); c.setTextDatum(lgfx::textdatum_t::top_left);
  char buffer[80];
  if (!boardReady) {
    text(c, "SE検出を確認", 8, 20, red, true); text(c, "Serialログを確認", 8, 57); text(c, "対象: K016-P-SE", 8, 82);
  } else switch (app.screen) {
    case Screen::Menu:
      if (app.selection == 5) {
        text(c, "設定", 8, 4, white, true); c.drawCircle(120, 67, 26, muted); c.drawCircle(120, 67, 10, mint);
      } else {
        text(c, names[app.selection], 8, 4, white, true); icon(c, Game(app.selection), 156, 71);
        const auto game = Game(app.selection); const auto mode = app.records.settings.mode;
        const auto& best = app.records.best[app.records.index(game, mode)];
        if (!best.valid) std::snprintf(buffer, sizeof(buffer), "ベスト --");
        else if (game == Game::Clock) std::snprintf(buffer, sizeof(buffer), "誤差%lu ms", static_cast<unsigned long>(best.value));
        else if (game == Game::Flight) std::snprintf(buffer, sizeof(buffer), "%lu.%lu秒", static_cast<unsigned long>(best.value / 10), static_cast<unsigned long>(best.value % 10));
        else std::snprintf(buffer, sizeof(buffer), "%lu", static_cast<unsigned long>(best.value));
        text(c, buffer, 8, 50, mint);
        if (game == Game::Flight) text(c, mode == Mode::Voice ? "音声モード" : "ボタンモード", 8, 79, muted);
      }
      std::snprintf(buffer, sizeof(buffer), "%u/6", app.selection + 1); text(c, buffer, 8, 94, muted);
      footer(c, "A:次へ B:決定"); break;
    case Screen::Instructions: {
      text(c, names[unsigned(app.game)], 8, 3, yellow, true);
      const char* first[5] = {"球が線に来たらA!", "Aでふくらませる", "A/Bの順番を覚える", "声で高さを変える", "10秒を感じて止める"};
      const char* second[5] = {"10球・後半ほど速い", "Bで割れる前に確定", "同じ順番で短く押す", "よけ続けて記録更新!", "途中で時計が隠れる"};
      text(c, app.game == Game::Flight && app.mode == Mode::Button ? "A保持で上昇" : first[unsigned(app.game)], 8, 37);
      text(c, second[unsigned(app.game)], 8, 60); icon(c, app.game, 194, 84);
      footer(c, "A:開始 B:戻る"); break;
    }
    case Screen::Countdown:
      std::snprintf(buffer, sizeof(buffer), "%lu", static_cast<unsigned long>(3 - std::min(uint32_t(2), app.screenElapsed(now) / 500)));
      number(c, buffer, 100, 39, yellow, 2); footer(c, "準備しよう"); break;
    case Screen::Play: gameRender(c, app, now); break;
    case Screen::Result: {
      const auto& r = app.result;
      text(c, r.timeout ? "TIME OUT" : r.perfect ? "PERFECT!" : r.burst ? "割れた!" : "結果", 8, 3, r.timeout || r.burst ? red : white, true);
      if (r.newBest) text(c, "NEW!", 178, 4, yellow);
      if (app.game == Game::Clock) {
        std::snprintf(buffer, sizeof(buffer), "%lu.%02lu", static_cast<unsigned long>(r.elapsedMs / 1000), static_cast<unsigned long>(r.elapsedMs % 1000 / 10)); number(c, buffer, 8, 33); text(c, "秒", 130, 39);
        std::snprintf(buffer, sizeof(buffer), "誤差 %lu ms", static_cast<unsigned long>(r.score)); text(c, buffer, 8, 66);
      } else if (app.game == Game::Flight) {
        std::snprintf(buffer, sizeof(buffer), "%lu.%lu", static_cast<unsigned long>(r.score / 10), static_cast<unsigned long>(r.score % 10)); number(c, buffer, 8, 33, yellow, r.score < 100000 ? 1.5f : 1.f);
        text(c, r.mode == Mode::Voice ? "秒 / 音声" : "秒 / ボタン", 8, 70);
      } else { std::snprintf(buffer, sizeof(buffer), "%lu", static_cast<unsigned long>(r.score)); number(c, buffer, 8, 36, yellow, 1.5f); text(c, app.game == Game::Memory ? "個 正解" : "点", 120, 53); }
      bestLabel(c, app, 91); footer(c, "A:再挑戦 B:選択へ"); break;
    }
    case Screen::Settings: {
      text(c, "設定", 8, 4, white, true);
      const char* settings[6] = {"効果音", "明るさ", "気球モード", "気球の再校正", "全記録リセット", "戻る"};
      text(c, settings[app.setting], 8, 42, yellow, true);
      if (app.setting == 0) text(c, app.records.settings.sound ? "ON" : "OFF", 8, 78);
      if (app.setting == 1) { std::snprintf(buffer, sizeof(buffer), "%u / 3", app.records.settings.brightness + 1); text(c, buffer, 8, 78); }
      if (app.setting == 2) text(c, app.records.settings.mode == Mode::Voice ? "音声" : "ボタン", 8, 78);
      footer(c, "A:項目送り B:変更"); break;
    }
    case Screen::ResetConfirm:
      text(c, "記録を消しますか?", 8, 16, red, true); text(c, "Bを2秒保持で確定", 8, 53);
      text(c, "設定は保持します", 8, 82, muted); footer(c, "A:取消 B:2秒保持"); break;
    case Screen::CalQuiet: case Screen::CalVoice:
      text(c, app.screen == Screen::CalQuiet ? "周りの音を測定" : "あー、と声を出して", 8, 15, yellow);
      text(c, app.screen == Screen::CalQuiet ? "2秒、静かに待とう" : "いつもの声で2秒", 8, 42, white, true);
      c.drawRect(10, 86, 220, 12, muted); c.fillRect(12, 88, int(std::min(uint32_t(2000), app.screenElapsed(now)) * 216 / 2000), 8, mint);
      footer(c, "B:最初から測り直す"); break;
    case Screen::CalDone:
      text(c, "校正できました", 8, 25, mint, true); text(c, "次の声プレイでも測定", 8, 65); footer(c, "A / B:設定へ"); break;
    case Screen::MicError:
      text(c, "声を確認できません", 8, 14, red); text(c, "周囲より少し大きく", 8, 44); text(c, "再測定かボタンで遊ぶ", 8, 70);
      footer(c, "A:再測定 B:ボタン/戻る"); break;
  }
  if (storageError) { c.fillCircle(231, 108, 3, red); }
}
}
