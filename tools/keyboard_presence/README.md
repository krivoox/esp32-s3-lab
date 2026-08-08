# Keyboard Presence Listener

Cuenta **cuánto tipeás** (sin guardar texto) y lo manda a la placa para el slide **Keyboard Presence**:

- teclas del día  
- WPM aproximado  
- tiempo idle  
- streak de días  
- heatmap semanal  
- mini Cubito que “escribe” en tiempo real  

---

## Requisitos

| Item | Notas |
|------|--------|
| macOS o Linux | En Windows no está probado |
| Python 3 | El `run.sh` crea un venv solo |
| `pynput` | Se instala solo vía `requirements.txt` |
| Placa online | Misma red **2.4 GHz** |
| **Accessibility** (macOS) | Obligatorio para escuchar el teclado |

Sin Accessibility, macOS imprime algo como:

```text
This process is not trusted! Input event monitoring will not be possible…
```

y la placa no recibe teclas reales (usá `--demo` para probar la UI).

---

## Setup (una vez)

### 1. IP de la placa

```bash
nano ~/.cursor/esp32-buddy.json
```

```json
{
  "url": "http://TU_IP/event"
}
```

### 2. Permiso Accessibility (macOS)

1. **System Settings → Privacy & Security → Accessibility**
2. Activá el proceso desde el que corrés el script:
   - **Terminal** / **iTerm** / **Cursor** / **Python** (según cómo lo lances)
3. Si ya estaba corriendo el listener, **cerralo y volvé a abrirlo** después de dar el permiso.

### 3. Dependencias

El wrapper se encarga:

```bash
./tools/keyboard_presence/run.sh --demo
```

Crea `tools/keyboard_presence/.venv` e instala `pynput` (carpeta gitignored).

---

## Uso

Desde la raíz del repo:

```bash
# Listener real (necesita Accessibility)
./tools/keyboard_presence/run.sh

# Demo: simula teclas sin pynput / sin permisos
./tools/keyboard_presence/run.sh --demo

# URL fija
./tools/keyboard_presence/run.sh --url http://192.168.0.15/keys
```

Salida típica:

```text
Keyboard Presence → http://192.168.0.15/keys
State: /Users/…/.cursor/keyboard-presence.json
Escuchando teclado (solo conteo). Ctrl+C para salir.
```

En la LCD: **BOOT** hasta el slide **Keys**. Al tipear, el contador sube y el mini buddy anima el caret.

Detener: `Ctrl+C`.

---

## Flags

| Flag | Descripción |
|------|-------------|
| `--demo` | Genera teclas falsas cada ~1 s (ideal para probar UI) |
| `--url URL` | Endpoint `…/keys` (override del JSON) |

---

## Cómo funciona

```text
Teclado (pynput) ──conteo──► tracker local
                                │
                    cada ~80 ms si hay delta
                                │
                                ▼
                 POST /keys  { delta, wpm, idle_s }
                                │
                 cada ~20 s snapshot { keys, week, streak, … }
                                ▼
                      Slide Keyboard Presence
```

### Qué se guarda en la Mac

Archivo: `~/.cursor/keyboard-presence.json`

```json
{
  "by_day": { "2026-08-08": 1204 },
  "streak": 3,
  "updated": "…"
}
```

Solo conteos por día. **Nunca** el texto de las teclas.

### Probar a mano (sin listener)

```bash
curl "http://TU_IP/keys?delta=20&wpm=45&idle_s=0"
curl "http://TU_IP/keys?keys=900&week=10,20,5,80,40,0,15&streak=3&wpm=35&idle_s=12"
```

---

## Troubleshooting

| Síntoma | Qué hacer |
|---------|-----------|
| `not trusted` / no cuenta teclas | Accessibility ON + reiniciar el script |
| `ModuleNotFoundError: pynput` | Usá `./run.sh` (no `python3` a pelo sin venv) |
| Placa no cambia | Estás en slide Keys? `BOOT`. IP correcta? |
| Contador no sube en demo | `--demo` manda solo; mirá `curl` al `/keys` |
| WPM raro | Es una estimación de ráfaga (~12 s), no un medidor profesional |

Ver en la placa:

```bash
curl -s http://TU_IP/ | sed -n '1,20p'
# keys_today: …
```

---

## Privacidad

- No se captura ni envía el contenido de lo que escribís.
- Solo métricas agregadas (conteos, WPM, idle, week CSV).
- Ideal para portfolio: el README puede decir “privacy-first keystroke metrics”.

---

## Archivos

| Archivo | Rol |
|---------|-----|
| `keyboard_presence.py` | Listener + POST |
| `run.sh` | venv + entrypoint |
| `requirements.txt` | `pynput` |
| `README.md` | Esta doc |

View relacionada: [`src/views/keyboard/`](../../src/views/keyboard/).
