#include "keyboard_presence.h"

#include "cursor_buddy.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace keyboard {
namespace {

constexpr uint32_t IDLE_AFTER_MS = 8000;
constexpr uint32_t AWAY_AFTER_MS = 180000;
constexpr uint32_t WPM_WINDOW_MS = 12000;
constexpr int MINI_PX = 3;

uint32_t week_keys[7] = {};
uint32_t keys_today = 0;
uint32_t streak_days = 0;
uint32_t last_key_ms = 0;
float wpm_hint = 0;
uint32_t idle_hint_s = 0;
bool have_hint = false;
uint32_t type_pulse_until = 0;
uint8_t type_tick = 0;

// ventana para WPM local
uint32_t window_keys = 0;
uint32_t window_start_ms = 0;

constexpr uint16_t COL_BG = 0x0000;
constexpr uint16_t COL_WHITE = 0xFFFF;
constexpr uint16_t COL_TOP = 0xC618;
constexpr uint16_t COL_SIDE = 0x7BEF;
constexpr uint16_t COL_INK = 0x0000;
constexpr uint16_t COL_MUTED = 0x8410;
constexpr uint16_t COL_OK = 0x07E0;
constexpr uint16_t COL_WARN = 0xFFE0;
constexpr uint16_t COL_ACCENT = 0x07FF;
constexpr uint16_t COL_HEAT0 = 0x18E3;
constexpr uint16_t COL_HEAT1 = 0x1B04;
constexpr uint16_t COL_HEAT2 = 0x0D80;
constexpr uint16_t COL_HEAT3 = 0x05E0;
constexpr uint16_t COL_HEAT4 = 0x07E0;
constexpr uint16_t COL_DIM = 0x4208;

uint16_t heatColor(uint32_t keys, uint32_t max_keys) {
  if (keys == 0 || max_keys == 0) {
    return COL_HEAT0;
  }
  const float t = constrain(static_cast<float>(keys) / static_cast<float>(max_keys), 0.f, 1.f);
  if (t < 0.2f) {
    return COL_HEAT1;
  }
  if (t < 0.45f) {
    return COL_HEAT2;
  }
  if (t < 0.7f) {
    return COL_HEAT3;
  }
  return COL_HEAT4;
}

const char* stateLabel(State s) {
  switch (s) {
    case State::Typing:
      return "typing";
    case State::Idle:
      return "idle";
    default:
      return "away";
  }
}

uint16_t stateColor(State s) {
  switch (s) {
    case State::Typing:
      return COL_OK;
    case State::Idle:
      return COL_WARN;
    default:
      return COL_MUTED;
  }
}

const uint8_t (*miniSprite(State st, uint32_t now_ms))[SPR_W] {
  if (st == State::Typing) {
    // Caret tipo “escribiendo”: alterna I-beam rápido; si recién hubo tecla, más vivo
    const bool hot = now_ms < type_pulse_until;
    const uint8_t phase = hot ? ((now_ms / 90) & 1) : ((now_ms / 160) & 1);
    return phase ? SPR_THINK_B : SPR_THINK_A;
  }
  if (st == State::Idle) {
    return ((now_ms / 500) % 10 >= 8) ? SPR_BLINK : SPR_IDLE;
  }
  // away: casi dormido
  return SPR_BLINK;
}

void drawMiniBuddy(Arduino_GFX* gfx, int left, int top, State st, uint32_t now_ms) {
  const auto* spr = miniSprite(st, now_ms);
  const bool dim = (st == State::Away);
  const int bob = (st == State::Typing) ? static_cast<int>(sinf(now_ms / 120.0f) * 2.0f) : 0;

  for (int y = 0; y < SPR_H; ++y) {
    for (int x = 0; x < SPR_W; ++x) {
      const uint8_t p = spr[y][x];
      if (p == 0) {
        continue;
      }
      uint16_t color = dim ? COL_DIM : COL_WHITE;
      if (p == 2) {
        color = dim ? COL_MUTED : COL_TOP;
      } else if (p == 3) {
        color = dim ? COL_DIM : COL_SIDE;
      } else if (p == 4) {
        color = COL_INK;
      }
      gfx->fillRect(left + x * MINI_PX, top + bob + y * MINI_PX, MINI_PX, MINI_PX, color);
    }
  }

  // Indicador “tap” bajo el cubo cuando hay tecla reciente
  if (st == State::Typing && now_ms < type_pulse_until) {
    const int cx = left + (SPR_W * MINI_PX) / 2;
    const int cy = top + SPR_H * MINI_PX + 4;
    gfx->fillCircle(cx, cy, 2, COL_OK);
  }
}

void parseWeekCsv(const char* csv) {
  if (!csv || !csv[0]) {
    return;
  }
  uint32_t tmp[7] = {};
  int idx = 0;
  const char* p = csv;
  while (*p && idx < 7) {
    char* end = nullptr;
    tmp[idx++] = static_cast<uint32_t>(strtoul(p, &end, 10));
    if (end == p) {
      break;
    }
    p = end;
    if (*p == ',') {
      ++p;
    }
  }
  if (idx == 7) {
    memcpy(week_keys, tmp, sizeof(week_keys));
  }
}

}  // namespace

void begin() {
  memset(week_keys, 0, sizeof(week_keys));
  keys_today = 0;
  streak_days = 0;
  last_key_ms = 0;
  window_keys = 0;
  window_start_ms = millis();
}

void update(uint32_t now_ms) {
  if (window_start_ms == 0) {
    window_start_ms = now_ms;
  }
  if (now_ms - window_start_ms > WPM_WINDOW_MS) {
    window_keys = 0;
    window_start_ms = now_ms;
  }
}

void applyDelta(uint32_t delta) {
  if (delta == 0) {
    return;
  }
  const uint32_t now = millis();
  keys_today += delta;
  week_keys[6] += delta;
  last_key_ms = now;
  have_hint = false;
  type_pulse_until = now + 450;
  ++type_tick;

  if (now - window_start_ms > WPM_WINDOW_MS) {
    window_keys = 0;
    window_start_ms = now;
  }
  window_keys += delta;
}

void setHints(float wpm_in, uint32_t idle_s) {
  wpm_hint = wpm_in;
  idle_hint_s = idle_s;
  have_hint = true;
}

void applySnapshot(uint32_t today, const char* week_csv, uint32_t streak,
                   float wpm_in, uint32_t idle_s) {
  keys_today = today;
  streak_days = streak;
  parseWeekCsv(week_csv);
  if (week_csv && week_csv[0]) {
    // alinear hoy con slot [6]
    week_keys[6] = today;
  } else {
    week_keys[6] = today;
  }
  wpm_hint = wpm_in;
  idle_hint_s = idle_s;
  have_hint = true;
  if (idle_s == 0 && today > 0) {
    last_key_ms = millis();
  }
}

uint32_t keysToday() { return keys_today; }

float wpm() {
  if (have_hint && wpm_hint > 0.5f) {
    return wpm_hint;
  }
  const uint32_t now = millis();
  const uint32_t elapsed = max<uint32_t>(now - window_start_ms, 1);
  // teclas/min / 5 ≈ WPM (asume ~5 chars/palabra)
  const float keys_per_min = window_keys * 60000.0f / static_cast<float>(elapsed);
  return keys_per_min / 5.0f;
}

uint32_t idleSeconds(uint32_t now_ms) {
  if (have_hint && state(now_ms) != State::Typing) {
    return idle_hint_s;
  }
  if (last_key_ms == 0) {
    return idle_hint_s > 0 ? idle_hint_s : (now_ms / 1000);
  }
  if (now_ms <= last_key_ms) {
    return 0;
  }
  return (now_ms - last_key_ms) / 1000;
}

State state(uint32_t now_ms) {
  if (last_key_ms == 0 && keys_today == 0) {
    return State::Away;
  }
  const uint32_t since = (last_key_ms == 0) ? now_ms : (now_ms - last_key_ms);
  if (since < IDLE_AFTER_MS) {
    return State::Typing;
  }
  if (since < AWAY_AFTER_MS) {
    return State::Idle;
  }
  return State::Away;
}

uint32_t streak() { return streak_days; }

const uint32_t* week() { return week_keys; }

void draw(Arduino_GFX* gfx, int width, int height, uint32_t now_ms, bool wifi_ok,
          const char* ip_line) {
  gfx->fillScreen(COL_BG);

  gfx->setTextSize(1);
  gfx->setTextColor(COL_WHITE);
  gfx->setCursor(8, 8);
  gfx->print("Keyboard");
  gfx->setTextColor(COL_MUTED);
  gfx->setCursor(8, 20);
  gfx->print("presence");

  const State st = state(now_ms);
  gfx->setTextColor(stateColor(st));
  gfx->setCursor(8, 36);
  gfx->print(stateLabel(st));

  gfx->setTextColor(wifi_ok ? COL_OK : COL_WARN);
  gfx->setCursor(8, 50);
  gfx->print(wifi_ok ? ip_line : "wifi?");

  // Contador grande
  char buf[24];
  snprintf(buf, sizeof(buf), "%lu", static_cast<unsigned long>(keys_today));
  gfx->setTextSize(3);
  gfx->setTextColor(COL_WHITE);
  const int tw = static_cast<int>(strlen(buf)) * 18;
  gfx->setCursor(max(8, (width - tw) / 2), 78);
  gfx->print(buf);

  gfx->setTextSize(1);
  gfx->setTextColor(COL_MUTED);
  gfx->setCursor((width - 54) / 2, 108);
  gfx->print("keys today");

  // WPM + idle (izquierda) + mini buddy tipeando (derecha)
  snprintf(buf, sizeof(buf), "%.0f wpm", static_cast<double>(wpm()));
  gfx->setTextSize(2);
  gfx->setTextColor(COL_ACCENT);
  gfx->setCursor(8, 130);
  gfx->print(buf);

  const uint32_t idle = idleSeconds(now_ms);
  if (idle < 60) {
    snprintf(buf, sizeof(buf), "idle %lus", static_cast<unsigned long>(idle));
  } else {
    snprintf(buf, sizeof(buf), "idle %lum", static_cast<unsigned long>(idle / 60));
  }
  gfx->setTextSize(1);
  gfx->setTextColor(COL_MUTED);
  gfx->setCursor(8, 152);
  gfx->print(buf);

  snprintf(buf, sizeof(buf), "streak %lu d", static_cast<unsigned long>(streak_days));
  gfx->setCursor(8, 166);
  gfx->print(buf);

  const int mini_w = SPR_W * MINI_PX;
  const int mini_x = width - mini_w - 6;
  const int mini_y = 128;
  drawMiniBuddy(gfx, mini_x, mini_y, st, now_ms);

  // Heatmap tipo contributions (7 días)
  gfx->setTextColor(COL_WHITE);
  gfx->setCursor(8, 190);
  gfx->print("week");

  uint32_t max_k = 1;
  for (int i = 0; i < 7; ++i) {
    if (week_keys[i] > max_k) {
      max_k = week_keys[i];
    }
  }

  const int cell = 18;
  const int gap = 4;
  const int total_w = 7 * cell + 6 * gap;
  const int start_x = (width - total_w) / 2;
  const int start_y = 208;
  static const char* days = "MTWTFSS";
  for (int i = 0; i < 7; ++i) {
    const int cx = start_x + i * (cell + gap);
    gfx->fillRoundRect(cx, start_y, cell, cell, 3, heatColor(week_keys[i], max_k));
    gfx->setTextColor(COL_MUTED);
    gfx->setCursor(cx + 6, start_y + cell + 4);
    gfx->print(days[i]);
  }

  // Barra de actividad reciente
  const float act = constrain(window_keys / 40.0f, 0.f, 1.f);
  const int bar_w = width - 16;
  gfx->drawRect(8, 250, bar_w, 10, COL_MUTED);
  gfx->fillRect(8, 250, static_cast<int>(bar_w * act), 10, COL_OK);

  gfx->setTextColor(COL_MUTED);
  gfx->setCursor(8, height - 28);
  gfx->print("POST /keys");
  gfx->setCursor(8, height - 14);
  gfx->print("BOOT = next slide");
}

}  // namespace keyboard
