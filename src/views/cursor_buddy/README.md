# Cursor Buddy — view

Slide 1 del carrusel (**BOOT**). Cubo isométrico estilo Cursor + globos de eventos del Agent + barras de uso Pro.

## Qué muestra

- Animación del buddy (idle / think / blink / look)
- Globo con eventos: `prompt`, `edit`, `shell`, `done`, etc.
- Barras **Cursor Models** / **Other Models** (porcentaje Pro)
- Aviso `quota!` cuando alguna barra llega a ≥95%

## Archivos

| Archivo | Rol |
|---------|-----|
| `cursor_buddy.h` | Sprites del cubo |
| Lógica de draw/eventos | hoy en `src/main.cpp` (orquestador) |

## Datos

- Hooks Cursor → `POST /event` (script usuario `~/.cursor/hooks/esp32-buddy-notify.py`)
- Uso Pro → `POST /usage` vía [`tools/cursor_usage/`](../../../tools/cursor_usage/)

## Pins / display

Ver `src/core/board_pins.h`. LCD 172×320 ST7789.
