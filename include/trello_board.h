#pragma once

#include <Arduino.h>
#include <Arduino_GFX_Library.h>

namespace trello {

static constexpr int MAX_ITEMS = 4;
static constexpr int TITLE_LEN = 22;
static constexpr int DUE_LEN = 10;

struct Item {
  char title[TITLE_LEN];
  char due[DUE_LEN];
  bool overdue;
};

void begin();
void apply(uint8_t n_progress, uint8_t n_due, uint8_t n_overdue, const Item* progress,
           uint8_t progress_count, const Item* due, uint8_t due_count);

uint8_t nProgress();
uint8_t nDue();
uint8_t nOverdue();

void draw(Arduino_GFX* gfx, int width, int height, uint32_t now_ms, bool wifi_ok,
          const char* ip_line);

}  // namespace trello
