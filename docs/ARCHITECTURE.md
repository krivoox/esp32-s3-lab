# Architecture

Firmware único (PlatformIO + Arduino) con **carrusel de views** en el LCD. El botón **BOOT** (GPIO0) cambia de slide; **RESET** reinicia el chip.

```text
Mac tools ──HTTP──► ESP32 WebServer ──► view activa en LCD
Cursor hooks          /event /usage
Keyboard listener     /keys
Trello poller         /trello
```

## Layout del repo

```text
src/
  main.cpp                 # orquestador: Wi‑Fi, HTTP, BOOT, loop
  core/                    # pines + IMU
  views/
    cursor_buddy/          # slide Agent + uso Pro
    keyboard/              # slide tipado
    trello/                # slide Trello
tools/
  cursor_usage/            # poller Pro
  keyboard_presence/       # listener teclado
  trello/                  # poller Trello
include/
  secrets.h.example        # plantilla Wi‑Fi (nunca secrets.h real)
docs/                      # ideas y notas
.cursor/rules/             # steering para agentes
```

## Añadir una view nueva (obligatorio)

Toda visual nueva **debe** ser un slide del carrusel BOOT, en carpeta propia y documentada.

1. Carpeta `src/views/<nombre>/` con el código de la view
2. `src/views/<nombre>/README.md` (qué muestra, archivos, datos/endpoints)
3. `enum class Slide` + `draw` / banner / LED en `main.cpp`
4. `-Isrc/views/<nombre>` en `platformio.ini`
5. Actualizar tabla de views en `README.md` raíz (+ `docs/IDEAS.md` si aplica)
6. Endpoint HTTP si necesita datos externos
7. Tool opcional en `tools/<nombre>/` con su README + `run.sh`

Regla de steering: `.cursor/rules/new-view-slide.mdc`.
