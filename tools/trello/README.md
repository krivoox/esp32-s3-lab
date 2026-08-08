# Trello poller

Sincroniza cards In Progress + due dates hacia `POST /trello`.

```bash
cp tools/trello/trello.json.example ~/.cursor/trello.json
# editá key, token, board
./tools/trello/run.sh --once
./tools/trello/run.sh --demo --once
```

Credenciales: [trello.com/power-ups/admin](https://trello.com/power-ups/admin).

**Nunca** commits `trello.json` con key/token reales. Usá `~/.cursor/trello.json`.
