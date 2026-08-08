# Keyboard Presence — view

Slide 2 del carrusel (**BOOT**). Estadísticas reales de tipado + mini Cubito que “escribe”.

## Qué muestra

- Teclas del día, WPM, idle, streak
- Heatmap semanal (tipo contributions)
- Mini buddy: caret rápido en `typing`, idle suave, away tenue

## Archivos

| Archivo | Rol |
|---------|-----|
| `keyboard_presence.h` / `.cpp` | Estado, métricas y draw |
| Usa sprites de | `../cursor_buddy/cursor_buddy.h` |

## Datos

Listener Mac → `POST /keys` (solo conteos, **nunca texto**):

```bash
./tools/keyboard_presence/run.sh
./tools/keyboard_presence/run.sh --demo
```

macOS: Accessibility para Python/Terminal.

Config URL: `~/.cursor/esp32-buddy.json` (misma IP que el buddy).
