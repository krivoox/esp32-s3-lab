# Trello Buddy — view

Slide 3 del carrusel (**BOOT**). Muñeco-tarjeta azul + cards In Progress y vencimientos.

## Qué muestra

- Header estilo board Trello
- Globo de estado (`active` / `due soon` / `overdue!`)
- Cards blancas con franja de acento y chip de fecha
- LED naranja si hay overdue

## Archivos

| Archivo | Rol |
|---------|-----|
| `trello_buddy.h` | Sprites del muñeco |
| `trello_board.h` / `.cpp` | Estado HTTP + UI |

## Datos

```bash
cp tools/trello/trello.json.example ~/.cursor/trello.json
# key + token reales (NO en el repo)
./tools/trello/run.sh --once
./tools/trello/run.sh --demo --once
```

Endpoint: `POST /trello` con campos `np`, `nd`, `no`, `p0`…, `d0`…

## Privacidad

Solo nombres de cards y fechas agregadas. Credenciales Trello **solo** en `~/.cursor/trello.json` (gitignored si está bajo `tools/**/trello.json`).
