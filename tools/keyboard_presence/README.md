# Keyboard presence listener

Cuenta teclas en macOS/Linux y manda deltas a `POST /keys`.

```bash
./tools/keyboard_presence/run.sh          # requiere pynput + Accessibility en macOS
./tools/keyboard_presence/run.sh --demo   # simula sin listener
```

Estado local: `~/.cursor/keyboard-presence.json` (solo conteos por día).

**Privacidad:** no captura ni envía el texto escrito.
