# Security

Este repo está pensado para ser **público**. Reglas:

## Nunca commitear

| Secreto | Dónde va (local) |
|---------|------------------|
| Wi‑Fi SSID/pass | `include/secrets.h` (gitignored) |
| Trello key/token | `~/.cursor/trello.json` |
| Cursor access token | solo lectura en vivo desde SQLite de Cursor; no se guarda en el repo |
| `.env` | gitignored |

Plantillas OK: `include/secrets.h.example`, `tools/trello/trello.json.example` (placeholders).

## Checklist antes de push / hacer público

```bash
# no debe haber archivos de secretos trackeados
git ls-files | rg -i 'secrets\.h$|trello\.json$|\.env'

# no debe haber tokens reales en el árbol
rg -n 'ATTA[0-9A-Za-z]+|WIFI_PASS\s+\"[^\"]{8,}\"' --glob '!.pio/**' --glob '!.git/**'
```

Si alguna vez se filtró un token: **revocalo** (Trello admin / Cursor logout-login) y rota Wi‑Fi si aplica.

## Datos que sí viajan a la placa

- Eventos de Agent (tipo/label, sin prompts completos del usuario en el diseño actual)
- Conteos de teclas (sin texto)
- % de uso Pro
- Nombres de cards Trello + fechas de vencimiento
