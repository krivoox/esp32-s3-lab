# ESP32-S3 Lab — Cursor Buddy

Waveshare **ESP32-S3-LCD-1.47B** + logo Cursor (cubo iso B/N) + eventos del Agent vía hooks.

## Qué hace

La placa se conecta al Wi‑Fi 2.4 GHz, publica `http://cursor-buddy.local/event` y muestra en pantalla lo que va pasando en Cursor (prompt, tools, subagents, done).

## Setup

1. Copiá `include/secrets.h.example` → `include/secrets.h` con tu Wi‑Fi **2.4 GHz**
2. `pio run -t upload`
3. Hooks de usuario ya viven en `~/.cursor/hooks.json` (valen para **todos** los proyectos)
4. Si mDNS no resuelve, editá `~/.cursor/esp32-buddy.json` con la IP que muestra la pantalla:

```json
{ "url": "http://192.168.1.50/event" }
```

## Probar a mano

```bash
curl "http://cursor-buddy.local/event?type=stop&label=done"
```

## Seguridad

`include/secrets.h` está en `.gitignore` (no subir la contraseña del Wi‑Fi).
