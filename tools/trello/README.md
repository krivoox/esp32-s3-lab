# Trello Poller

Sincroniza tu tablero Trello con el slide **Trello Buddy** de la placa:

- cards en listas tipo **In Progress / Doing / WIP**
- cards con **fecha de vencimiento** (próximos N días + overdue)
- muñeco serio + LED naranja si hay overdue

---

## Requisitos

- Cuenta Trello
- API **key** + **token** ([Power-Ups Admin](https://trello.com/power-ups/admin))
- Nombre (o id) del board
- Placa online en Wi‑Fi **2.4 GHz**
- Python 3 (stdlib only)

---

## Setup (una vez)

### 1. Credenciales Trello

1. Abrí [https://trello.com/power-ups/admin](https://trello.com/power-ups/admin)
2. Creá / abrí una Power-Up → generá **API key**
3. En la misma página, generá el **Token** (Allow) y copialo

### 2. Archivo de config (fuera del repo)

```bash
cp tools/trello/trello.json.example ~/.cursor/trello.json
nano ~/.cursor/trello.json
```

```json
{
  "key": "TU_API_KEY_REAL",
  "token": "TU_TOKEN_REAL",
  "board": "Nombre exacto del tablero",
  "in_progress_lists": ["In Progress", "Doing", "En progreso", "WIP"],
  "due_within_days": 7
}
```

| Campo | Descripción |
|-------|-------------|
| `key` | API key de Trello |
| `token` | Token de usuario (no lo subas a git) |
| `board` | Nombre exacto **o** id del board |
| `in_progress_lists` | Nombres de listas que cuentan como “en curso” (match parcial case-insensitive) |
| `due_within_days` | Ventana de vencimientos futuros (default `7`) |

> **Importante:** no pongas key/token en `tools/trello/trello.json.example` ni en el repo.  
> El path que lee el script es **`~/.cursor/trello.json`**.

### 3. IP de la placa

```bash
nano ~/.cursor/esp32-buddy.json
```

```json
{
  "url": "http://TU_IP/event"
}
```

---

## Uso

Desde la raíz del repo:

```bash
# Sync real una vez
./tools/trello/run.sh --once

# Loop cada 3 minutos (default 180 s)
./tools/trello/run.sh

# Demo sin API (datos fake para ver la UI)
./tools/trello/run.sh --demo --once

# URL fija
./tools/trello/run.sh --once --url http://192.168.0.15/trello
```

Salida esperada:

```text
Trello poller → http://192.168.0.15/trello
progress=2 due=5 overdue=1 board=ok
```

En la LCD: **BOOT** hasta el slide **Trello**.

---

## Flags

| Flag | Default | Descripción |
|------|---------|-------------|
| `--once` | off | Una corrida y exit |
| `--interval N` | `180` | Segundos entre syncs (mín. 60) |
| `--demo` | off | No llama a Trello; manda cards de ejemplo |
| `--url URL` | config / mDNS | Endpoint `…/trello` |

---

## Cómo funciona

```text
Trello REST API
  GET /boards/{id}/lists?cards=open
        │
        ├─ listas cuyo nombre ∈ in_progress_lists  → progress
        └─ cards con due (no complete) en ventana   → due / overdue
        │
        ▼
POST /trello  np, nd, no, p0, p0d, d0, d0d, d0o, …
        │
        ▼
Slide Trello Buddy
```

### Match de listas In Progress

Por defecto (y/o lo que pongas en JSON), se consideran listas cuyo nombre contiene (sin importar mayúsculas):

`in progress`, `doing`, `en progreso`, `wip`, `working`, `activo`

Si tu lista se llama distinto (ej. `Working on it`), agregala a `in_progress_lists`.

### Probar a mano

```bash
curl "http://TU_IP/trello?np=2&nd=1&no=1&p0=Ship+LCD&p1=Docs&d0=Pay+invoice&d0d=2d+late&d0o=1"
```

---

## Troubleshooting

| Síntoma | Qué hacer |
|---------|-----------|
| `Credenciales Trello incompletas` | Todavía hay placeholders; editá `~/.cursor/trello.json` |
| `401 Unauthorized` | Key/token mal o token revocado → regenerá en Power-Ups Admin |
| `Board '…' no encontrado` | Nombre exacto (case-insensitive) o usá el id del board |
| `progress=0` pero tenés cards | El nombre de la lista no matchea `in_progress_lists` |
| `due=0` | No hay dues, están complete, o fuera de `due_within_days` |
| `board=FAIL` | Placa offline / IP vieja en `esp32-buddy.json` |

Listar boards (debug rápido):

```bash
python3 - <<'PY'
import json, urllib.parse, urllib.request, pathlib
cfg=json.loads(pathlib.Path.home().joinpath('.cursor/trello.json').read_text())
q=urllib.parse.urlencode({'key':cfg['key'],'token':cfg['token'],'fields':'name,id','filter':'open'})
print(urllib.request.urlopen('https://api.trello.com/1/members/me/boards?'+q).read().decode())
PY
```

Ver en la placa:

```bash
curl -s http://TU_IP/ | sed -n '1,25p'
# trello progress/due: 2/5
```

---

## Privacidad y seguridad

- Se envían **nombres de cards** y labels de fecha (`today`, `2d late`, etc.), no descripciones ni attachments.
- Key/token **solo** en `~/.cursor/trello.json`.
- Si alguna vez pegaste un token en el repo: **revocalo** en Trello y generá uno nuevo.
- Ver [`docs/SECURITY.md`](../../docs/SECURITY.md).

---

## Archivos

| Archivo | Rol |
|---------|-----|
| `trello_presence.py` | Fetch Trello + POST |
| `run.sh` | Entrypoint |
| `trello.json.example` | Plantilla **sin** secretos |
| `README.md` | Esta doc |

View relacionada: [`src/views/trello/`](../../src/views/trello/).
