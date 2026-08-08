# ESP32-S3 Lab — Cursor Buddy + Keyboard Presence

Waveshare **ESP32-S3-LCD-1.47B**: carrusel de slides con **BOOT**.

| BOOT | Slide |
|------|--------|
| 1 | **Cursor Buddy** — eventos del Agent (hooks) |
| 2 | **Keyboard Presence** — tipado real, WPM, idle, heatmap |

## Setup

1. Copiá `include/secrets.h.example` → `include/secrets.h` con tu Wi‑Fi **2.4 GHz**
2. `pio run -t upload`
3. Hooks Cursor: `~/.cursor/hooks.json` → `POST /event`
4. Uso Pro (barras Cursor / Other en slide Buddy):
   ```bash
   ./tools/run-cursor-usage.sh          # cada 2 min
   ./tools/run-cursor-usage.sh --once   # una vez
   ```
5. Teclado (tiempo real en slide Keys):
   ```bash
   ./tools/run-keyboard-presence.sh
   ```
   En macOS: **System Settings → Privacy & Security → Accessibility** → activá
   **Python** (o Terminal/iTerm). Sin eso el OS bloquea el listener y la placa no se entera.
6. Si mDNS falla, poné la IP en `~/.cursor/esp32-buddy.json`:

```json
{ "url": "http://192.168.1.50/event" }
```

## Probar

```bash
curl "http://cursor-buddy.local/event?type=stop&label=done"
curl "http://cursor-buddy.local/usage?cursor=72&other=87"
curl "http://cursor-buddy.local/keys?delta=20&wpm=45&idle_s=0"
./tools/run-keyboard-presence.sh --demo
./tools/run-cursor-usage.sh --once
```

Al llegar a **≥95%** en cualquier barra, el buddy avisa con globo `quota!`.

## Seguridad

`include/secrets.h` está en `.gitignore`. El listener de teclado **no guarda texto**, solo conteos.

Más detalle: [`IDEAS.md`](IDEAS.md).
