#include "trello_board.h"

#include "trello_buddy.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

namespace trello {
namespace {

// Paleta tipo tablero Trello (oscuro + acentos)
constexpr uint16_t COL_BG = 0x10A2;          // azul noche suave
constexpr uint16_t COL_HEADER = 0x0259;      // azul Trello
constexpr uint16_t COL_HEADER_DEEP = 0x0134;
constexpr uint16_t COL_WHITE = 0xFFFF;
constexpr uint16_t COL_OFFWHITE = 0xEF7D;
constexpr uint16_t COL_MUTED = 0x9CF3;
constexpr uint16_t COL_OK = 0x07E0;
constexpr uint16_t COL_WARN = 0xFE60;
constexpr uint16_t COL_DANGER = 0xF92A;
constexpr uint16_t COL_TRELLO = 0x057F;
constexpr uint16_t COL_TRELLO_LIGHT = 0x45BF;
constexpr uint16_t COL_TRELLO_DARK = 0x0234;
constexpr uint16_t COL_EYE = 0x0861;         // azul muy oscuro (ojos)
constexpr uint16_t COL_CARD = 0xFFFF;        // cards blancas
constexpr uint16_t COL_CARD_TEXT = 0x18C3;
constexpr uint16_t COL_ACCENT_PROG = 0x07FF; // cyan
constexpr uint16_t COL_ACCENT_DUE = 0xFD20;  // naranja
constexpr uint16_t COL_PILL = 0x1A6E;
constexpr int PX = 3;

Item progress_items[MAX_ITEMS];
Item due_items[MAX_ITEMS];
uint8_t progress_count = 0;
uint8_t due_count = 0;
uint8_t n_progress = 0;
uint8_t n_due = 0;
uint8_t n_overdue = 0;
uint32_t updated_ms = 0;

void clearItems(Item* items, uint8_t& count) {
  count = 0;
  for (int i = 0; i < MAX_ITEMS; ++i) {
    items[i].title[0] = '\0';
    items[i].due[0] = '\0';
    items[i].overdue = false;
  }
}

void truncate(char* out, size_t out_n, const char* in, size_t max_chars) {
  strncpy(out, in, out_n - 1);
  out[out_n - 1] = '\0';
  if (strlen(out) > max_chars) {
    out[max_chars - 1] = '.';
    out[max_chars] = '\0';
  }
}

void drawSprite(Arduino_GFX* gfx, int left, int top, bool warn, uint32_t now_ms) {
  const bool blink = ((now_ms / 480) % 12) >= 10;
  const auto* spr = warn ? TRELLO_WARN : (blink ? TRELLO_BLINK : TRELLO_IDLE);
  const int bob = static_cast<int>(sinf(now_ms / 320.0f) * 1.5f);

  // sombra suave
  gfx->fillRoundRect(left + 2, top + bob + TRELLO_SPR_H * PX - 2, TRELLO_SPR_W * PX - 2, 4, 2,
                     COL_HEADER_DEEP);

  for (int y = 0; y < TRELLO_SPR_H; ++y) {
    for (int x = 0; x < TRELLO_SPR_W; ++x) {
      const uint8_t p = spr[y][x];
      if (p == 0) {
        continue;
      }
      uint16_t color = COL_TRELLO;
      if (p == 2) {
        color = COL_TRELLO_LIGHT;
      } else if (p == 3) {
        color = COL_TRELLO_DARK;
      } else if (p == 4) {
        color = COL_EYE;
      }
      gfx->fillRect(left + x * PX, top + bob + y * PX, PX, PX, color);
    }
  }
}

void drawPill(Arduino_GFX* gfx, int x, int y, const char* text, uint16_t bg, uint16_t fg) {
  const int tw = static_cast<int>(strlen(text)) * 6;
  const int w = tw + 10;
  const int h = 12;
  gfx->fillRoundRect(x, y, w, h, 6, bg);
  gfx->setTextSize(1);
  gfx->setTextColor(fg);
  gfx->setCursor(x + 5, y + 2);
  gfx->print(text);
}

void drawSectionTitle(Arduino_GFX* gfx, int y, const char* title, uint16_t dot, int count,
                      int width) {
  gfx->fillCircle(12, y + 4, 3, dot);
  gfx->setTextSize(1);
  gfx->setTextColor(COL_OFFWHITE);
  gfx->setCursor(20, y);
  gfx->print(title);

  char pill[8];
  snprintf(pill, sizeof(pill), "%d", count);
  const int tw = static_cast<int>(strlen(pill)) * 6 + 10;
  drawPill(gfx, width - 8 - tw, y - 1, pill, COL_PILL, COL_WHITE);
}

void drawCard(Arduino_GFX* gfx, int y, const Item& it, uint16_t accent, bool emphasize_due) {
  const int x = 8;
  const int w = 156;
  const int h = it.due[0] ? 28 : 20;

  // sombra
  gfx->fillRoundRect(x + 1, y + 2, w, h, 5, COL_HEADER_DEEP);
  // cuerpo blanco
  gfx->fillRoundRect(x, y, w, h, 5, COL_CARD);
  // franja de acento
  gfx->fillRoundRect(x, y, 4, h, 2, it.overdue ? COL_DANGER : accent);
  gfx->fillRect(x + 2, y, 2, h, it.overdue ? COL_DANGER : accent);

  char title[TITLE_LEN];
  truncate(title, sizeof(title), it.title, 18);
  gfx->setTextSize(1);
  gfx->setTextColor(COL_CARD_TEXT);
  gfx->setCursor(x + 9, y + 4);
  gfx->print(title);

  if (it.due[0]) {
    const uint16_t due_col = it.overdue ? COL_DANGER : (emphasize_due ? COL_ACCENT_DUE : COL_MUTED);
    // chip de fecha
    char due_s[DUE_LEN];
    truncate(due_s, sizeof(due_s), it.due, 9);
    const int chip_w = static_cast<int>(strlen(due_s)) * 6 + 8;
    gfx->fillRoundRect(x + 8, y + 15, chip_w, 10, 3, it.overdue ? 0xF9EF : 0xEF5D);
    gfx->setTextColor(due_col);
    gfx->setCursor(x + 12, y + 16);
    gfx->print(due_s);
  }
}

void drawStatusBubble(Arduino_GFX* gfx, int x, int y, const char* msg, uint16_t color) {
  const int tw = static_cast<int>(strlen(msg)) * 6;
  const int w = constrain(tw + 14, 48, 110);
  const int h = 18;
  gfx->fillRoundRect(x, y, w, h, 8, COL_WHITE);
  // colita
  gfx->fillTriangle(x + w - 2, y + 8, x + w + 6, y + 4, x + w + 6, y + 14, COL_WHITE);
  gfx->setTextSize(1);
  gfx->setTextColor(color);
  gfx->setCursor(x + 7, y + 5);
  gfx->print(msg);
}

}  // namespace

void begin() {
  clearItems(progress_items, progress_count);
  clearItems(due_items, due_count);
  n_progress = n_due = n_overdue = 0;
  updated_ms = 0;
}

void apply(uint8_t np, uint8_t nd, uint8_t no, const Item* progress, uint8_t pc,
           const Item* due, uint8_t dc) {
  n_progress = np;
  n_due = nd;
  n_overdue = no;
  clearItems(progress_items, progress_count);
  clearItems(due_items, due_count);
  progress_count = pc < MAX_ITEMS ? pc : MAX_ITEMS;
  due_count = dc < MAX_ITEMS ? dc : MAX_ITEMS;
  for (uint8_t i = 0; i < progress_count; ++i) {
    progress_items[i] = progress[i];
  }
  for (uint8_t i = 0; i < due_count; ++i) {
    due_items[i] = due[i];
  }
  updated_ms = millis();
}

uint8_t nProgress() { return n_progress; }
uint8_t nDue() { return n_due; }
uint8_t nOverdue() { return n_overdue; }

void draw(Arduino_GFX* gfx, int width, int height, uint32_t now_ms, bool wifi_ok,
          const char* ip_line) {
  gfx->fillScreen(COL_BG);

  // Header band (sensación de board Trello)
  gfx->fillRect(0, 0, width, 78, COL_HEADER);
  gfx->fillRect(0, 72, width, 6, COL_HEADER_DEEP);

  // Decoración: franjas tipo listas
  gfx->fillRoundRect(8, 70, 28, 4, 2, COL_TRELLO_LIGHT);
  gfx->fillRoundRect(40, 70, 20, 4, 2, 0x34BF);
  gfx->fillRoundRect(64, 70, 16, 4, 2, 0xFE60);

  gfx->setTextSize(1);
  gfx->setTextColor(COL_WHITE);
  gfx->setCursor(10, 8);
  gfx->print("Trello");
  gfx->setTextColor(wifi_ok ? COL_OK : COL_WARN);
  gfx->setCursor(10, 20);
  char ip_short[16];
  truncate(ip_short, sizeof(ip_short), wifi_ok ? ip_line : "wifi?", 14);
  gfx->print(ip_short);

  const bool warn = n_overdue > 0;
  const int spr_w = TRELLO_SPR_W * PX;
  drawSprite(gfx, width - spr_w - 6, 10, warn, now_ms);

  char bubble[22];
  uint16_t bubble_col = COL_TRELLO_DARK;
  if (n_overdue > 0) {
    snprintf(bubble, sizeof(bubble), "%u overdue!", static_cast<unsigned>(n_overdue));
    bubble_col = COL_DANGER;
  } else if (n_due > 0) {
    snprintf(bubble, sizeof(bubble), "%u due soon", static_cast<unsigned>(n_due));
    bubble_col = COL_ACCENT_DUE;
  } else if (n_progress > 0) {
    snprintf(bubble, sizeof(bubble), "%u active", static_cast<unsigned>(n_progress));
    bubble_col = COL_TRELLO;
  } else {
    snprintf(bubble, sizeof(bubble), "all clear");
    bubble_col = COL_MUTED;
  }
  drawStatusBubble(gfx, 10, 38, bubble, bubble_col);

  // Contenido
  int y = 88;
  drawSectionTitle(gfx, y, "In progress", COL_ACCENT_PROG, n_progress, width);
  y += 16;

  if (progress_count == 0) {
    gfx->fillRoundRect(8, y, 156, 22, 5, COL_PILL);
    gfx->setTextColor(COL_MUTED);
    gfx->setCursor(16, y + 7);
    gfx->print("Nada en curso");
    y += 30;
  } else {
    const uint8_t show = progress_count < 2 ? progress_count : 2;
    for (uint8_t i = 0; i < show; ++i) {
      drawCard(gfx, y, progress_items[i], COL_ACCENT_PROG, false);
      y += progress_items[i].due[0] ? 32 : 24;
    }
    y += 4;
  }

  drawSectionTitle(gfx, y, "Due dates", n_overdue ? COL_DANGER : COL_ACCENT_DUE, n_due, width);
  y += 16;

  if (due_count == 0) {
    gfx->fillRoundRect(8, y, 156, 22, 5, COL_PILL);
    gfx->setTextColor(COL_MUTED);
    gfx->setCursor(16, y + 7);
    gfx->print("Sin vencimientos");
  } else {
    for (uint8_t i = 0; i < due_count && i < 3; ++i) {
      if (y > height - 42) {
        break;
      }
      drawCard(gfx, y, due_items[i], COL_ACCENT_DUE, true);
      y += due_items[i].due[0] ? 32 : 24;
    }
  }

  gfx->setTextColor(COL_MUTED);
  gfx->setCursor(10, height - 12);
  gfx->print("BOOT next");
}

}  // namespace trello
