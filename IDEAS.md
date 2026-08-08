# Ideas — ESP32-S3 Lab

Carrusel de “apps” en un solo firmware. El botón **BOOT** (GPIO0) cambia de slide; **RESET** no sirve para eso.

```text
BOOT → siguiente modo
```

## Slides

| # | Slide | Estado |
|---|--------|--------|
| 1 | **Cursor Buddy** | Hecho — eventos + globo + barras de uso Pro |
| 2 | **Keyboard Presence Detector** | Hecho — heatmap + WPM + idle + mini buddy |
| 3 | Reloj / otros | Pendiente |

---

## Keyboard Presence Detector

Detecta presencia real frente al teclado (tipado, no apps).

### Qué mide

- **Cuánto escribís** — teclas por día
- **Velocidad** — WPM aproximado (ventana ~12 s)
- **Tiempo idle** — segundos sin tocar teclado
- **Heatmap semanal** — 7 celdas tipo GitHub Contributions
- **Streak** — días seguidos con actividad

### Endpoints

```bash
# delta de teclas
curl "http://cursor-buddy.local/keys?delta=12&wpm=40&idle_s=0"

# snapshot (heatmap)
curl "http://cursor-buddy.local/keys?keys=900&week=10,20,5,80,40,0,15&streak=3&wpm=35&idle_s=12"
```

Privacidad: solo conteos, **nunca el texto**.

### Listener Mac

```bash
pip3 install pynput
python3 tools/keyboard_presence.py
# o demo sin Accessibility:
python3 tools/keyboard_presence.py --demo
```

Estado local: `~/.cursor/keyboard-presence.json`  
URL: lee `~/.cursor/esp32-buddy.json` (misma IP que Cursor Buddy).
