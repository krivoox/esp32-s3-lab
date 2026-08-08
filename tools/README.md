# tools/

Pollers y listeners que corren en la Mac y alimentan la placa por HTTP.

| Tool | Slide | Endpoint | README |
|------|-------|----------|--------|
| [`cursor_usage/`](cursor_usage/) | Cursor Buddy | `POST /usage` | [docs](cursor_usage/README.md) |
| [`keyboard_presence/`](keyboard_presence/) | Keyboard | `POST /keys` | [docs](keyboard_presence/README.md) |
| [`trello/`](trello/) | Trello | `POST /trello` | [docs](trello/README.md) |

## Config compartida de la placa

Creá (o editá) `~/.cursor/esp32-buddy.json` con la IP que muestra la LCD:

```json
{
  "url": "http://192.168.0.15/event",
  "urls": [
    "http://192.168.0.15/event",
    "http://cursor-buddy.local/event"
  ]
}
```

Cada tool deriva solo el path (`/usage`, `/keys`, `/trello`).

## Convención

Cada tool tiene:

- `README.md` — instrucciones completas
- `run.sh` — entrypoint
- script `.py` — lógica
- `*.example` si necesita secretos (nunca commits reales)
