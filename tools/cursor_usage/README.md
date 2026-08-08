# Cursor Usage Poller

Publica en la placa el **% de uso de tu plan Pro** (las mismas dos barras del settings de Cursor):

| Barra en LCD | Campo API | Significado |
|--------------|-----------|-------------|
| **Cursor** | `autoPercentUsed` | Cursor Models (Grok, Composer, etc.) |
| **Other** | `apiPercentUsed` | Other Models |

Se muestra en el slide **Cursor Buddy**. Si alguna barra llega a **≥ 95%**, el buddy avisa con globo `quota!`.

---

## Requisitos

- macOS (lee la DB local de Cursor)
- Cursor instalado y **logueado**
- Placa flasheada y en la misma red Wi‑Fi **2.4 GHz**
- Python 3 (stdlib only; no hace falta `pip`)

---

## Setup (una vez)

### 1. IP de la placa

Mirá la IP en la LCD (línea verde) y creá/actualizá:

```bash
nano ~/.cursor/esp32-buddy.json
```

```json
{
  "url": "http://TU_IP/event"
}
```

Ejemplo: `"url": "http://192.168.0.15/event"`.

### 2. Verificar que Cursor tiene token

El poller lee (solo lectura):

`~/Library/Application Support/Cursor/User/globalStorage/state.vscdb`  
→ key `cursorAuth/accessToken`

Si Cursor no está logueado, vas a ver un error de token.

---

## Uso

Desde la raíz del repo:

```bash
# Una sola actualización
./tools/cursor_usage/run.sh --once

# Loop cada 2 minutos (default)
./tools/cursor_usage/run.sh

# Loop cada 5 minutos
./tools/cursor_usage/run.sh --interval 300

# Forzar URL
./tools/cursor_usage/run.sh --once --url http://192.168.0.15/usage
```

Salida esperada:

```text
Usage poller → http://192.168.0.15/usage (every 120s)
cursor=73% other=87% board=ok
```

En la LCD (slide Cursor) deberías ver las dos barras abajo.

---

## Flags

| Flag | Default | Descripción |
|------|---------|-------------|
| `--once` | off | Corre una vez y sale |
| `--interval N` | `120` | Segundos entre polls (mín. efectivo 30) |
| `--url URL` | desde config / mDNS | Endpoint `…/usage` |

---

## Cómo funciona

```text
Cursor SQLite (accessToken)
        │
        ▼
api2.cursor.sh  GetCurrentPeriodUsage
        │
        ▼
POST http://<placa>/usage  { "cursor": 73, "other": 87 }
        │
        ▼
Barras en slide Cursor Buddy
```

- **No** guarda el token en disco.
- **No** imprime el token.
- Si POST falla, reintenta con GET querystring.

### Probar a mano (sin poller)

```bash
curl "http://TU_IP/usage?cursor=72&other=87"
# aviso de cuota (demo):
curl "http://TU_IP/usage?cursor=96&other=80"
```

---

## Troubleshooting

| Síntoma | Qué revisar |
|---------|-------------|
| `No cursorAuth/accessToken` | Abrí Cursor y asegurate de estar logueado |
| `board=FAIL` / timeout | Misma Wi‑Fi 2.4 GHz; IP en `esp32-buddy.json`; placa online |
| Barras en `--` | Todavía no llegó ningún `/usage`; corré `--once` |
| % raros / 0 | Plan distinto o API cambió; mirá serial / HTTP root de la placa |
| mDNS no resuelve | Usá IP directa en el JSON, no `cursor-buddy.local` |

Ver estado en la placa:

```bash
curl -s http://TU_IP/ | sed -n '1,20p'
# busca: usage cursor/other: 73/87
```

---

## Privacidad y seguridad

- Solo se envían dos enteros (0–100).
- El token de Cursor **nunca** va al repo ni a la placa.
- Ver también [`docs/SECURITY.md`](../../docs/SECURITY.md).

---

## Archivos

| Archivo | Rol |
|---------|-----|
| `cursor_usage.py` | Lógica del poller |
| `run.sh` | Entrypoint |
| `README.md` | Esta doc |

View relacionada: [`src/views/cursor_buddy/`](../../src/views/cursor_buddy/).
