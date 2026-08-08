# ESP32-S3 Lab — Cursor Buddy + Keyboard + Trello

Waveshare **ESP32-S3-LCD-1.47B**: carrusel de slides con **BOOT**.

| BOOT | Slide |
|------|--------|
| 1 | **Cursor Buddy** — eventos del Agent + barras Pro |
| 2 | **Keyboard Presence** — tipado, WPM, idle, heatmap |
| 3 | **Trello** — In Progress + vencimientos |

## Setup

1. Copiá `include/secrets.h.example` → `include/secrets.h` con tu Wi‑Fi **2.4 GHz**
2. `pio run -t upload`
3. Hooks Cursor: `~/.cursor/hooks.json` → `POST /event`
4. Uso Pro:
   ```bash
   ./tools/run-cursor-usage.sh
   ```
5. Teclado:
   ```bash
   ./tools/run-keyboard-presence.sh
   ```
   macOS: Accessibility → Python/Terminal.
6. Trello:
   ```bash
   cp tools/trello.json.example ~/.cursor/trello.json
   # key + token: https://trello.com/power-ups/admin
   ./tools/run-trello.sh --once
   ./tools/run-trello.sh --demo --once   # sin API
   ```
7. IP en `~/.cursor/esp32-buddy.json` si mDNS falla.

## Probar

```bash
curl "http://192.168.0.15/trello?np=2&nd=1&no=1&p0=Ship+LCD&d0=Pay+invoice&d0d=2d+late&d0o=1"
./tools/run-trello.sh --demo --once
```

Avisos: uso Pro ≥95% → `quota!`. Cards overdue → muñeco serio + LED naranja.
