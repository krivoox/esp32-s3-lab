# Cursor usage poller

Lee el access token **local** de Cursor (SQLite) y publica uso Pro a la placa.

```bash
./tools/cursor_usage/run.sh --once
./tools/cursor_usage/run.sh --interval 120
```

- API: `GetCurrentPeriodUsage` → `autoPercentUsed` (Cursor) + `apiPercentUsed` (Other)
- Destino: `POST /usage` (URL desde `~/.cursor/esp32-buddy.json`)

No guarda ni imprime el token.
