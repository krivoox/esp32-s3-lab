#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

// Keyboard Presence Detector — métricas agregadas (sin texto).
namespace keyboard {

enum class State : uint8_t { Away, Idle, Typing };

void begin();
void update(uint32_t now_ms);

// delta: teclas nuevas desde el último POST
// snapshot opcional: today + week CSV "a,b,c,d,e,f,g" + streak
void applyDelta(uint32_t delta);
void applySnapshot(uint32_t today, const char* week_csv, uint32_t streak,
                   float wpm_hint, uint32_t idle_s);
void setHints(float wpm_hint, uint32_t idle_s);

uint32_t keysToday();
float wpm();
uint32_t idleSeconds(uint32_t now_ms);
State state(uint32_t now_ms);
uint32_t streak();
const uint32_t* week();  // 7 días, [6] = hoy

void draw(Arduino_GFX* gfx, int width, int height, uint32_t now_ms,
          bool wifi_ok, const char* ip_line);

}  // namespace keyboard
