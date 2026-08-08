#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include <ESPmDNS.h>
#include <FastLED.h>
#include <WebServer.h>
#include <WiFi.h>
#include <math.h>
#include <string.h>

#include "board_pins.h"
#include "cursor_buddy.h"
#include "keyboard_presence.h"
#include "qmi8658.h"
#include "secrets.h"

namespace {

Arduino_DataBus* bus = new Arduino_ESP32SPI(
    PIN_LCD_DC, PIN_LCD_CS, PIN_LCD_SCLK, PIN_LCD_MOSI, GFX_NOT_DEFINED);

Arduino_ST7789* display = new Arduino_ST7789(
    bus, PIN_LCD_RST, 0, true, LCD_WIDTH, LCD_HEIGHT, LCD_COL_OFFSET, 0, LCD_COL_OFFSET, 0);

Arduino_Canvas* gfx = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, display);

WebServer server(80);
Qmi8658 imu;
CRGB led;

enum class Slide : uint8_t {
  CursorBuddy = 0,
  KeyboardPresence = 1,
  Count = 2,
};

enum class Gesture : uint8_t {
  Idle,
  Blink,
  Happy,
  LookLeft,
  LookRight,
  Think,
};

Slide slide = Slide::CursorBuddy;
bool boot_prev = true;
uint32_t boot_ignore_until = 0;
uint32_t slide_banner_until = 0;

bool imu_ok = false;
bool wifi_ok = false;
float smooth_ax = 0;
float smooth_ay = 0;
float energy = 0;
float prev_acc_mag = 1;
uint32_t last_ms = 0;
uint32_t anim_ms = 0;
uint32_t gesture_until = 0;
uint32_t event_until = 0;
uint8_t anim_tick = 0;
Gesture gesture = Gesture::Idle;
char status_line[28] = "boot";
char detail_line[28] = "";
char ip_line[20] = "";
int8_t spark_x[8];
int8_t spark_y[8];
uint8_t spark_life[8];

// Uso Pro: Cursor Models / Other Models (0–100). -1 = sin datos.
int8_t usage_cursor_pct = -1;
int8_t usage_other_pct = -1;
bool usage_warned = false;
uint32_t usage_updated_ms = 0;

constexpr uint16_t COL_BG = 0x0000;
constexpr uint16_t COL_WHITE = 0xFFFF;
constexpr uint16_t COL_TOP = 0xC618;
constexpr uint16_t COL_SIDE = 0x7BEF;
constexpr uint16_t COL_INK = 0x0000;
constexpr uint16_t COL_MUTED = 0x8410;
constexpr uint16_t COL_OK = 0x07E0;
constexpr uint16_t COL_WARN = 0xFFE0;
constexpr uint16_t COL_DANGER = 0xF800;
constexpr uint16_t COL_BUBBLE = 0xFFFF;
constexpr uint16_t COL_BUBBLE_EDGE = 0x0000;
constexpr uint16_t COL_BAR_CURSOR = 0x7D7C;  // azul claro
constexpr uint16_t COL_BAR_OTHER = 0xC618;   // gris plata
constexpr uint16_t COL_BAR_BG = 0x2104;

constexpr int PX = 6;
constexpr int USAGE_WARN_PCT = 95;  // aviso cuando falta ≤5%

void setBacklight(uint8_t percent) {
  pinMode(PIN_LCD_BL, OUTPUT);
  ledcSetup(0, 5000, 8);
  ledcAttachPin(PIN_LCD_BL, 0);
  ledcWrite(0, map(constrain(percent, 0, 100), 0, 100, 0, 255));
}

void setStatus(const char* status, const char* detail = "") {
  strncpy(status_line, status, sizeof(status_line) - 1);
  status_line[sizeof(status_line) - 1] = '\0';
  strncpy(detail_line, detail, sizeof(detail_line) - 1);
  detail_line[sizeof(detail_line) - 1] = '\0';
  event_until = millis() + 6500;
}

void nextSlide() {
  slide = static_cast<Slide>(
      (static_cast<uint8_t>(slide) + 1) % static_cast<uint8_t>(Slide::Count));
  slide_banner_until = millis() + 1200;
  Serial.printf("Slide -> %u\n", static_cast<unsigned>(slide));
}

void pollBoot() {
  const uint32_t now = millis();
  if (now < boot_ignore_until) {
    boot_prev = digitalRead(PIN_BOOT) != LOW;
    return;
  }
  const bool pressed = digitalRead(PIN_BOOT) == LOW;
  if (pressed && !boot_prev) {
    nextSlide();
    boot_ignore_until = now + 280;
  }
  boot_prev = pressed;
}

void drawSpeechBubble(int tip_x, int tip_y, const char* line1, const char* line2) {
  if (!line1 || !line1[0]) {
    return;
  }

  char l1[16];
  char l2[18];
  strncpy(l1, line1, sizeof(l1) - 1);
  l1[sizeof(l1) - 1] = '\0';
  if (strlen(l1) > 12) {
    l1[11] = '.';
    l1[12] = '\0';
  }

  const bool has_l2 = line2 && line2[0];
  if (has_l2) {
    strncpy(l2, line2, sizeof(l2) - 1);
    l2[sizeof(l2) - 1] = '\0';
    if (strlen(l2) > 16) {
      l2[15] = '.';
      l2[16] = '\0';
    }
  } else {
    l2[0] = '\0';
  }

  const int pad_x = 8;
  const int pad_y = 6;
  const int gap = 4;
  const int tw1 = static_cast<int>(strlen(l1)) * 12;
  const int tw2 = has_l2 ? static_cast<int>(strlen(l2)) * 6 : 0;
  const int content_w = max(tw1, tw2);
  const int box_w = constrain(content_w + pad_x * 2, 56, LCD_WIDTH - 8);
  const int box_h = has_l2 ? (pad_y * 2 + 16 + gap + 8) : (pad_y * 2 + 16);
  const int tail_h = 10;

  int box_x = tip_x - box_w / 2;
  box_x = constrain(box_x, 4, LCD_WIDTH - box_w - 4);
  int box_y = tip_y - box_h - tail_h;
  if (box_y < 40) {
    box_y = 40;
  }

  gfx->fillRoundRect(box_x, box_y, box_w, box_h, 8, COL_BUBBLE);
  gfx->drawRoundRect(box_x, box_y, box_w, box_h, 8, COL_BUBBLE_EDGE);
  gfx->drawRoundRect(box_x + 1, box_y + 1, box_w - 2, box_h - 2, 7, COL_BUBBLE_EDGE);

  const int tx = constrain(tip_x, box_x + 14, box_x + box_w - 14);
  const int ty = box_y + box_h;
  gfx->fillTriangle(tx - 7, ty - 1, tx + 7, ty - 1, tip_x, tip_y, COL_BUBBLE);
  gfx->drawLine(tx - 7, ty - 1, tip_x, tip_y, COL_BUBBLE_EDGE);
  gfx->drawLine(tx + 7, ty - 1, tip_x, tip_y, COL_BUBBLE_EDGE);

  gfx->setTextSize(2);
  gfx->setTextColor(COL_INK);
  gfx->setCursor(box_x + (box_w - tw1) / 2, box_y + pad_y);
  gfx->print(l1);

  if (has_l2) {
    gfx->setTextSize(1);
    gfx->setTextColor(COL_MUTED);
    gfx->setCursor(box_x + (box_w - tw2) / 2, box_y + pad_y + 16 + gap);
    gfx->print(l2);
  }
}

void startGesture(Gesture g, uint32_t duration_ms) {
  gesture = g;
  gesture_until = millis() + duration_ms;
}

void applyUsage(int cursor_pct, int other_pct) {
  usage_cursor_pct = static_cast<int8_t>(constrain(cursor_pct, 0, 100));
  usage_other_pct = static_cast<int8_t>(constrain(other_pct, 0, 100));
  usage_updated_ms = millis();

  const bool low = usage_cursor_pct >= USAGE_WARN_PCT || usage_other_pct >= USAGE_WARN_PCT;
  if (low && !usage_warned) {
    usage_warned = true;
    char detail[20];
    if (usage_cursor_pct >= USAGE_WARN_PCT && usage_other_pct >= USAGE_WARN_PCT) {
      snprintf(detail, sizeof(detail), "both ~%d%%",
               static_cast<int>(usage_cursor_pct > usage_other_pct ? usage_cursor_pct
                                                                   : usage_other_pct));
    } else if (usage_cursor_pct >= USAGE_WARN_PCT) {
      snprintf(detail, sizeof(detail), "Cursor %d%%", static_cast<int>(usage_cursor_pct));
    } else {
      snprintf(detail, sizeof(detail), "Other %d%%", static_cast<int>(usage_other_pct));
    }
    setStatus("quota!", detail);
    startGesture(Gesture::Think, 2800);
    Serial.printf("USAGE WARN cursor=%d other=%d\n", usage_cursor_pct, usage_other_pct);
  } else if (!low) {
    usage_warned = false;
  }
}

void drawUsageBar(int y, const char* label, int pct, uint16_t fill) {
  gfx->setTextSize(1);
  gfx->setTextColor(COL_MUTED);
  gfx->setCursor(10, y);
  gfx->print(label);

  char pct_s[8];
  if (pct < 0) {
    snprintf(pct_s, sizeof(pct_s), "--");
  } else {
    snprintf(pct_s, sizeof(pct_s), "%d%%", pct);
  }
  gfx->setTextColor(pct >= USAGE_WARN_PCT ? COL_DANGER : COL_WHITE);
  gfx->setCursor(LCD_WIDTH - 10 - static_cast<int>(strlen(pct_s)) * 6, y);
  gfx->print(pct_s);

  const int bar_y = y + 11;
  const int bar_w = LCD_WIDTH - 20;
  const int bar_h = 7;
  gfx->fillRoundRect(10, bar_y, bar_w, bar_h, 2, COL_BAR_BG);
  if (pct > 0) {
    const int fill_w = max(2, (bar_w * constrain(pct, 0, 100)) / 100);
    const uint16_t color = pct >= USAGE_WARN_PCT ? COL_DANGER : (pct >= 80 ? COL_WARN : fill);
    gfx->fillRoundRect(10, bar_y, fill_w, bar_h, 2, color);
  }
}

void drawUsagePanel() {
  const int top = LCD_HEIGHT - 68;
  drawUsageBar(top, "Cursor", usage_cursor_pct, COL_BAR_CURSOR);
  drawUsageBar(top + 26, "Other", usage_other_pct, COL_BAR_OTHER);
}

void spawnSparks(int cx, int cy) {
  for (int i = 0; i < 8; ++i) {
    spark_x[i] = static_cast<int8_t>(cx - 32 + random(0, 64));
    spark_y[i] = static_cast<int8_t>(cy - 12 + random(0, 28));
    spark_life[i] = static_cast<uint8_t>(18 + random(0, 22));
  }
}

void applyAgentEvent(const String& type, const String& label, const String& tool) {
  String t = type;
  t.toLowerCase();
  String lab = label.length() ? label : tool;
  if (lab.length() > 22) {
    lab = lab.substring(0, 22);
  }

  if (t == "session" || t == "sessionstart") {
    setStatus("session", lab.c_str());
    startGesture(Gesture::Idle, 1200);
  } else if (t == "sessionend") {
    setStatus("bye", lab.c_str());
    startGesture(Gesture::Blink, 800);
  } else if (t == "prompt" || t == "beforesubmitprompt") {
    setStatus("prompt", lab.c_str());
    startGesture(Gesture::Think, 2000);
  } else if (t == "tool" || t == "pretooluse" || t == "posttooluse") {
    String tool_l = tool;
    tool_l.toLowerCase();
    setStatus(lab.length() ? lab.c_str() : "tool", tool.c_str());
    if (tool_l.indexOf("shell") >= 0) {
      startGesture(Gesture::LookRight, 1400);
    } else if (tool_l.indexOf("write") >= 0 || tool_l.indexOf("edit") >= 0) {
      startGesture(Gesture::LookLeft, 1400);
    } else if (tool_l.indexOf("task") >= 0 || tool_l.indexOf("agent") >= 0) {
      startGesture(Gesture::Think, 1800);
    } else {
      startGesture(Gesture::Think, 1200);
    }
  } else if (t == "subagent" || t == "subagentstart") {
    setStatus("agent", lab.c_str());
    startGesture(Gesture::Think, 2500);
  } else if (t == "subagentstop") {
    setStatus("agent ok", lab.c_str());
    startGesture(Gesture::Happy, 1600);
  } else if (t == "stop" || t == "done") {
    setStatus("done", lab.c_str());
    startGesture(Gesture::Happy, 2200);
    spawnSparks(LCD_WIDTH / 2, 110);
  } else if (t == "thought" || t == "afteragentthought") {
    setStatus("think", lab.c_str());
    startGesture(Gesture::Think, 1500);
  } else if (t == "response" || t == "afteragentresponse") {
    setStatus("reply", lab.c_str());
    startGesture(Gesture::Blink, 1000);
  } else {
    setStatus(t.c_str(), lab.c_str());
    startGesture(Gesture::Think, 1200);
  }

  Serial.printf("EVENT type=%s label=%s tool=%s\n", type.c_str(), label.c_str(), tool.c_str());
}

String jsonGet(const String& body, const char* key) {
  String pattern = String("\"") + key + "\":";
  int i = body.indexOf(pattern);
  if (i < 0) {
    return "";
  }
  i += pattern.length();
  while (i < static_cast<int>(body.length()) && (body[i] == ' ')) {
    ++i;
  }
  if (i >= static_cast<int>(body.length())) {
    return "";
  }
  if (body[i] == '"') {
    ++i;
    int j = body.indexOf('"', i);
    if (j < 0) {
      return "";
    }
    return body.substring(i, j);
  }
  int j = i;
  while (j < static_cast<int>(body.length()) &&
         body[j] != ',' && body[j] != '}' && body[j] != ' ') {
    ++j;
  }
  return body.substring(i, j);
}

void handleRoot() {
  String html = F("<!doctype html><html><body style='font-family:sans-serif;background:#000;color:#fff'>"
                  "<h1>ESP32-S3 Lab</h1><p>POST /event · POST /keys</p><pre>");
  html += "ip: ";
  html += WiFi.localIP().toString();
  html += "\nslide: ";
  html += (slide == Slide::CursorBuddy) ? "cursor" : "keyboard";
  html += "\nstatus: ";
  html += status_line;
  html += "\nkeys_today: ";
  html += String(keyboard::keysToday());
  html += "\nusage cursor/other: ";
  html += String(usage_cursor_pct);
  html += "/";
  html += String(usage_other_pct);
  html += "</pre></body></html>";
  server.send(200, "text/html", html);
}

void handleEvent() {
  if (server.method() == HTTP_OPTIONS) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(204);
    return;
  }

  String body = server.arg("plain");
  if (!body.length()) {
    body = server.arg("body");
  }
  String type = server.hasArg("type") ? server.arg("type") : jsonGet(body, "type");
  String label = server.hasArg("label") ? server.arg("label") : jsonGet(body, "label");
  String tool = server.hasArg("tool") ? server.arg("tool") : jsonGet(body, "tool");

  if (!type.length()) {
    type = "event";
  }
  applyAgentEvent(type, label, tool);

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleKeys() {
  if (server.method() == HTTP_OPTIONS) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(204);
    return;
  }

  String body = server.arg("plain");
  if (!body.length()) {
    body = server.arg("body");
  }

  auto argOrJson = [&](const char* key) -> String {
    if (server.hasArg(key)) {
      return server.arg(key);
    }
    return jsonGet(body, key);
  };

  const String delta_s = argOrJson("delta");
  const String keys_s = argOrJson("keys");
  const String week_s = argOrJson("week");
  const String streak_s = argOrJson("streak");
  const String wpm_s = argOrJson("wpm");
  const String idle_s = argOrJson("idle_s");

  // Snapshot completo si viene week (el Mac manda el heatmap).
  if (week_s.length()) {
    const uint32_t today =
        keys_s.length() ? static_cast<uint32_t>(keys_s.toInt()) : keyboard::keysToday();
    const uint32_t streak = streak_s.length() ? static_cast<uint32_t>(streak_s.toInt()) : 0;
    const float wpm_v = wpm_s.length() ? wpm_s.toFloat() : 0;
    const uint32_t idle = idle_s.length() ? static_cast<uint32_t>(idle_s.toInt()) : 0;
    keyboard::applySnapshot(today, week_s.c_str(), streak, wpm_v, idle);
  } else if (delta_s.length()) {
    const long d = delta_s.toInt();
    keyboard::applyDelta(static_cast<uint32_t>(d < 0 ? 0 : d));
    if (wpm_s.length() || idle_s.length()) {
      keyboard::setHints(wpm_s.length() ? wpm_s.toFloat() : 0,
                         idle_s.length() ? static_cast<uint32_t>(idle_s.toInt()) : 0);
    }
  } else if (keys_s.length()) {
    // curl simple: ?keys=5 → delta
    const long d = keys_s.toInt();
    keyboard::applyDelta(static_cast<uint32_t>(d < 0 ? 0 : d));
  }

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleUsage() {
  if (server.method() == HTTP_OPTIONS) {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.sendHeader("Access-Control-Allow-Methods", "POST, GET, OPTIONS");
    server.sendHeader("Access-Control-Allow-Headers", "Content-Type");
    server.send(204);
    return;
  }

  String body = server.arg("plain");
  if (!body.length()) {
    body = server.arg("body");
  }

  auto argOrJson = [&](const char* key) -> String {
    if (server.hasArg(key)) {
      return server.arg(key);
    }
    return jsonGet(body, key);
  };

  // cursor = Cursor Models %, other = Other Models %
  String cursor_s = argOrJson("cursor");
  if (!cursor_s.length()) {
    cursor_s = argOrJson("auto");
  }
  String other_s = argOrJson("other");
  if (!other_s.length()) {
    other_s = argOrJson("api");
  }

  const int cursor_pct = cursor_s.length() ? cursor_s.toInt() : usage_cursor_pct;
  const int other_pct = other_s.length() ? other_s.toInt() : usage_other_pct;
  applyUsage(cursor_pct < 0 ? 0 : cursor_pct, other_pct < 0 ? 0 : other_pct);

  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", "{\"ok\":true}");
}

void handleNotFound() {
  server.send(404, "text/plain", "not found");
}

bool connectWifi() {
  setStatus("wifi...", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true, true);
  delay(200);
  WiFi.setSleep(false);
  WiFi.setHostname("cursor-buddy");

  Serial.println("WiFi scan...");
  const int n = WiFi.scanNetworks(/*async=*/false, /*hidden=*/true);
  bool saw_target = false;
  for (int i = 0; i < n; ++i) {
    const String ssid = WiFi.SSID(i);
    Serial.printf("  [%d] %s  rssi=%d  ch=%d\n", i, ssid.c_str(), WiFi.RSSI(i),
                  WiFi.channel(i));
    if (ssid == WIFI_SSID || ssid.indexOf("jmkrivo") >= 0) {
      saw_target = true;
    }
  }
  if (!saw_target) {
    Serial.printf("Target SSID not seen: '%s'\n", WIFI_SSID);
  }

  WiFi.begin(WIFI_SSID, WIFI_PASS);
  Serial.printf("Connecting to '%s'...\n", WIFI_SSID);

  const uint32_t start = millis();
  wl_status_t st = WL_IDLE_STATUS;
  while ((st = WiFi.status()) != WL_CONNECTED && millis() - start < 30000) {
    delay(400);
    Serial.printf(".");
    if ((millis() - start) % 4000 < 450) {
      Serial.printf(" st=%d\n", static_cast<int>(st));
    }
  }
  Serial.println();

  if (WiFi.status() != WL_CONNECTED) {
    Serial.printf("WiFi fail status=%d\n", static_cast<int>(WiFi.status()));
    setStatus("wifi fail", WIFI_SSID);
    snprintf(ip_line, sizeof(ip_line), "no ip");
    return false;
  }

  snprintf(ip_line, sizeof(ip_line), "%s", WiFi.localIP().toString().c_str());
  setStatus("online", ip_line);
  Serial.printf("WiFi OK %s\n", ip_line);

  if (MDNS.begin("cursor-buddy")) {
    MDNS.addService("http", "tcp", 80);
    Serial.println("mDNS: http://cursor-buddy.local");
  }

  server.on("/", HTTP_GET, handleRoot);
  server.on("/event", HTTP_POST, handleEvent);
  server.on("/event", HTTP_GET, handleEvent);
  server.on("/event", HTTP_OPTIONS, handleEvent);
  server.on("/keys", HTTP_POST, handleKeys);
  server.on("/keys", HTTP_GET, handleKeys);
  server.on("/keys", HTTP_OPTIONS, handleKeys);
  server.on("/usage", HTTP_POST, handleUsage);
  server.on("/usage", HTTP_GET, handleUsage);
  server.on("/usage", HTTP_OPTIONS, handleUsage);
  server.onNotFound(handleNotFound);
  server.begin();
  return true;
}

const uint8_t (*currentSprite())[SPR_W] {
  switch (gesture) {
    case Gesture::Blink:
      return SPR_BLINK;
    case Gesture::Happy:
      return SPR_HAPPY;
    case Gesture::LookLeft:
      return SPR_LOOK_L;
    case Gesture::LookRight:
      return SPR_LOOK_R;
    case Gesture::Think:
      return (anim_tick & 1) ? SPR_THINK_B : SPR_THINK_A;
    case Gesture::Idle:
    default:
      if ((anim_tick % 10) >= 8) {
        return SPR_BLINK;
      }
      return SPR_IDLE;
  }
}

void drawSprite(int left, int top) {
  const auto* spr = currentSprite();
  for (int y = 0; y < SPR_H; ++y) {
    for (int x = 0; x < SPR_W; ++x) {
      const uint8_t p = spr[y][x];
      if (p == 0) {
        continue;
      }
      uint16_t color = COL_WHITE;
      if (p == 2) {
        color = COL_TOP;
      } else if (p == 3) {
        color = COL_SIDE;
      } else if (p == 4) {
        color = COL_INK;
      }
      gfx->fillRect(left + x * PX, top + y * PX, PX, PX, color);
    }
  }
}

void drawSlideBanner(const char* title) {
  gfx->fillRoundRect(10, 130, LCD_WIDTH - 20, 36, 8, COL_BUBBLE);
  gfx->drawRoundRect(10, 130, LCD_WIDTH - 20, 36, 8, COL_BUBBLE_EDGE);
  gfx->setTextSize(2);
  gfx->setTextColor(COL_INK);
  const int tw = static_cast<int>(strlen(title)) * 12;
  gfx->setCursor(max(16, (LCD_WIDTH - tw) / 2), 140);
  gfx->print(title);
}

void drawCursorBuddy(uint32_t now) {
  gfx->fillScreen(COL_BG);

  gfx->setTextSize(1);
  gfx->setTextColor(COL_WHITE);
  gfx->setCursor(12, 10);
  gfx->print("Cursor");
  gfx->setTextColor(wifi_ok ? COL_OK : COL_WARN);
  gfx->setCursor(12, 24);
  gfx->print(wifi_ok ? ip_line : "wifi?");

  const int bob = static_cast<int>(sinf(now / 340.0f) * 2.0f);
  const int spr_w = SPR_W * PX;
  const int spr_h = SPR_H * PX;
  int x = (LCD_WIDTH - spr_w) / 2 + static_cast<int>(constrain(smooth_ax, -1.0f, 1.0f) * 8);
  int y = 100 + bob + static_cast<int>(constrain(smooth_ay, -1.0f, 1.0f) * 6);
  y = constrain(y, 70, LCD_HEIGHT - spr_h - 78);

  if (status_line[0] && (now < event_until || gesture != Gesture::Idle)) {
    drawSpeechBubble(x + spr_w / 2, y - 2, status_line, detail_line);
  }

  drawSprite(x, y);

  for (int i = 0; i < 8; ++i) {
    if (spark_life[i] == 0) {
      continue;
    }
    gfx->fillRect(spark_x[i], spark_y[i], 2, 2, COL_WHITE);
  }

  drawUsagePanel();
}

}  // namespace

void setup() {
  Serial.begin(115200);
  delay(400);
  Serial.println("ESP32-S3 Lab — slides + WiFi");

  pinMode(PIN_BOOT, INPUT_PULLUP);
  boot_ignore_until = millis() + 800;

  FastLED.addLeds<WS2812, PIN_RGB, GRB>(&led, 1);
  led = CRGB::White;
  FastLED.show();

  display->begin(40000000);
  setBacklight(100);
  display->fillScreen(COL_BG);
  gfx->begin();

  keyboard::begin();
  imu_ok = imu.begin(Wire, PIN_I2C_SDA, PIN_I2C_SCL);
  wifi_ok = connectWifi();

  last_ms = millis();
  anim_ms = millis();
  memset(spark_life, 0, sizeof(spark_life));
}

void loop() {
  if (wifi_ok) {
    server.handleClient();
  } else if (WiFi.status() != WL_CONNECTED && (millis() % 15000) < 30) {
    wifi_ok = connectWifi();
  }

  pollBoot();

  const uint32_t now = millis();
  float dt = (now - last_ms) / 1000.0f;
  if (dt <= 0 || dt > 0.05f) {
    dt = 0.016f;
  }
  last_ms = now;

  keyboard::update(now);

  if (now - anim_ms >= 300) {
    anim_ms = now;
    ++anim_tick;
  }

  if (gesture != Gesture::Idle && now >= gesture_until) {
    gesture = Gesture::Idle;
  }

  Vec3 accel{}, gyro{};
  if (imu_ok && imu.read(accel, gyro)) {
    smooth_ax = smooth_ax * 0.85f + accel.x * 0.15f;
    smooth_ay = smooth_ay * 0.85f + accel.y * 0.15f;
    const float gyro_mag =
        sqrtf(gyro.x * gyro.x + gyro.y * gyro.y + gyro.z * gyro.z);
    const float acc_mag =
        sqrtf(accel.x * accel.x + accel.y * accel.y + accel.z * accel.z);
    const float jolt = fabsf(acc_mag - prev_acc_mag) * 8.0f + gyro_mag / 45.0f;
    prev_acc_mag = acc_mag;
    energy = energy * 0.9f + min(jolt, 2.0f) * 0.1f;
  }

  for (int i = 0; i < 8; ++i) {
    if (spark_life[i] == 0) {
      continue;
    }
    spark_y[i] -= 1;
    --spark_life[i];
  }

  if (now < slide_banner_until) {
    gfx->fillScreen(COL_BG);
    drawSlideBanner(slide == Slide::CursorBuddy ? "Cursor" : "Keys");
  } else if (slide == Slide::KeyboardPresence) {
    keyboard::draw(gfx, LCD_WIDTH, LCD_HEIGHT, now, wifi_ok, ip_line);
  } else {
    drawCursorBuddy(now);
  }

  gfx->flush();

  const uint8_t bri = static_cast<uint8_t>(25 + constrain(energy * 140, 0, 200));
  if (slide == Slide::KeyboardPresence) {
    const auto st = keyboard::state(now);
    if (st == keyboard::State::Typing) {
      led = CRGB(0, bri, bri / 2);
    } else if (st == keyboard::State::Idle) {
      led = CRGB(bri, bri / 2, 0);
    } else {
      led = CRGB(bri / 3, bri / 3, bri / 3);
    }
  } else {
    led = wifi_ok ? CRGB(bri, bri, bri) : CRGB(bri, bri / 3, 0);
  }
  FastLED.show();
}
