#!/usr/bin/env python3
"""Trello → ESP32 Buddy (POST /trello).

Muestra cards en listas In Progress y vencimientos próximos/overdue.

Config: ~/.cursor/trello.json
{
  "key": "TRELLO_API_KEY",
  "token": "TRELLO_TOKEN",
  "board": "Nombre del board o id",
  "in_progress_lists": ["In Progress", "Doing", "En progreso", "WIP"]
}

Key/token: https://trello.com/power-ups/admin → generate
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
from datetime import datetime, timezone

CONFIG_PATH = os.path.expanduser("~/.cursor/trello.json")
BUDDY_PATH = os.path.expanduser("~/.cursor/esp32-buddy.json")

DEFAULT_PROGRESS_LISTS = (
    "in progress",
    "doing",
    "en progreso",
    "wip",
    "working",
    "activo",
)


def load_buddy_url() -> str:
    try:
        with open(BUDDY_PATH, "r", encoding="utf-8") as f:
            cfg = json.load(f)
        base = cfg.get("url") or (cfg.get("urls") or [None])[0]
        if isinstance(base, str) and base:
            for suffix in ("/event", "/usage", "/keys", "/trello"):
                if base.endswith(suffix):
                    return base[: -len(suffix)] + "/trello"
            return base.rstrip("/") + "/trello"
    except (OSError, json.JSONDecodeError, TypeError, IndexError):
        pass
    return "http://cursor-buddy.local/trello"


def load_trello_cfg() -> dict:
    if not os.path.exists(CONFIG_PATH):
        raise RuntimeError(
            f"Falta {CONFIG_PATH}. Crealo con key, token y board (ver tools/trello/trello.json.example)."
        )
    with open(CONFIG_PATH, "r", encoding="utf-8") as f:
        cfg = json.load(f)
    key = str(cfg.get("key") or "").strip()
    token = str(cfg.get("token") or "").strip()
    board = str(cfg.get("board") or "").strip()
    if (not key or not token or key.startswith("TU_") or token.startswith("TU_")
            or "API_KEY" in key.upper() or board.startswith("Nombre")):
        raise RuntimeError(
            "Credenciales Trello incompletas en ~/.cursor/trello.json\n"
            "1) Key:  https://trello.com/power-ups/admin  → New → Generate API key\n"
            "2) Token: en esa misma página, 'Token' → Allow (copiá el token)\n"
            "3) board: nombre exacto de tu tablero (o su id)\n"
            "4) Volvé a correr: ./tools/trello/run.sh --once"
        )
    cfg["key"] = key
    cfg["token"] = token
    cfg["board"] = board
    return cfg


def trello_get(path: str, key: str, token: str, params: dict | None = None) -> object:
    q = {"key": key, "token": token}
    if params:
        q.update(params)
    url = f"https://api.trello.com/1{path}?{urllib.parse.urlencode(q)}"
    req = urllib.request.Request(url, headers={"User-Agent": "esp32-trello-buddy/1.0"})
    try:
        with urllib.request.urlopen(req, timeout=25) as resp:
            return json.loads(resp.read().decode())
    except urllib.error.HTTPError as e:
        body = e.read().decode("utf-8", "replace")[:200]
        if e.code == 401:
            raise RuntimeError(
                "Trello 401 Unauthorized: key/token inválidos o token expirado.\n"
                "Regenerá token en https://trello.com/power-ups/admin y actualizá "
                "~/.cursor/trello.json"
            ) from e
        raise RuntimeError(f"Trello HTTP {e.code}: {body}") from e


def resolve_board_id(cfg: dict) -> str:
    board = str(cfg.get("board") or "").strip()
    if not board:
        raise RuntimeError("Configurá 'board' (nombre o id)")
    if len(board) >= 16 and " " not in board:
        return board
    boards = trello_get("/members/me/boards", cfg["key"], cfg["token"], {"fields": "name,id", "filter": "open"})
    assert isinstance(boards, list)
    needle = board.lower()
    for b in boards:
        if str(b.get("name", "")).lower() == needle:
            return str(b["id"])
    for b in boards:
        if needle in str(b.get("name", "")).lower():
            return str(b["id"])
    names = ", ".join(str(b.get("name")) for b in boards[:12])
    raise RuntimeError(f"Board '{board}' no encontrado. Tenés: {names}")


def fmt_due(due_iso: str | None) -> tuple[str, bool, int]:
    """Return (label, overdue, sort_key_days)."""
    if not due_iso:
        return "", False, 9999
    try:
        due = datetime.fromisoformat(due_iso.replace("Z", "+00:00"))
    except ValueError:
        return due_iso[:9], False, 9999
    now = datetime.now(timezone.utc)
    delta = due - now
    days = int(delta.total_seconds() // 86400)
    overdue = days < 0
    if overdue:
        label = f"{-days}d late" if days > -30 else "late"
    elif days == 0:
        label = "today"
    elif days == 1:
        label = "tomorrow"
    else:
        label = f"in {days}d"
    return label, overdue, days


def collect(cfg: dict) -> dict:
    key, token = cfg["key"], cfg["token"]
    board_id = resolve_board_id(cfg)
    lists = trello_get(
        f"/boards/{board_id}/lists",
        key,
        token,
        {"cards": "open", "card_fields": "name,due,dueComplete", "filter": "open"},
    )
    assert isinstance(lists, list)

    progress_names = [
        s.lower()
        for s in (cfg.get("in_progress_lists") or list(DEFAULT_PROGRESS_LISTS))
    ]
    due_within = int(cfg.get("due_within_days") or 7)

    progress_cards: list[dict] = []
    due_cards: list[dict] = []

    for lst in lists:
        lname = str(lst.get("name") or "").lower()
        is_progress = any(p == lname or p in lname for p in progress_names)
        for card in lst.get("cards") or []:
            name = str(card.get("name") or "").strip()
            if not name:
                continue
            due = card.get("due")
            due_complete = bool(card.get("dueComplete"))
            label, overdue, days = fmt_due(due if isinstance(due, str) else None)
            item = {"title": name[:21], "due": label[:9], "overdue": overdue}

            if is_progress:
                progress_cards.append(item)

            if due and not due_complete and (overdue or days <= due_within):
                due_cards.append({**item, "_days": days})

    due_cards.sort(key=lambda c: (not c["overdue"], c.get("_days", 9999)))
    n_overdue = sum(1 for c in due_cards if c["overdue"])

    return {
        "np": len(progress_cards),
        "nd": len(due_cards),
        "no": n_overdue,
        "progress": progress_cards[:4],
        "due": due_cards[:4],
    }


def post_board(url: str, data: dict) -> bool:
    payload: dict[str, object] = {
        "np": data["np"],
        "nd": data["nd"],
        "no": data["no"],
    }
    for i, card in enumerate(data["progress"][:4]):
        payload[f"p{i}"] = card["title"]
        if card.get("due"):
            payload[f"p{i}d"] = card["due"]
        if card.get("overdue"):
            payload[f"p{i}o"] = 1
    for i, card in enumerate(data["due"][:4]):
        payload[f"d{i}"] = card["title"]
        if card.get("due"):
            payload[f"d{i}d"] = card["due"]
        if card.get("overdue"):
            payload[f"d{i}o"] = 1

    body = json.dumps(payload).encode()
    req = urllib.request.Request(
        url, data=body, headers={"Content-Type": "application/json"}, method="POST"
    )
    try:
        with urllib.request.urlopen(req, timeout=2.5) as resp:
            return 200 <= getattr(resp, "status", 200) < 300
    except (urllib.error.URLError, TimeoutError, OSError):
        qs = urllib.parse.urlencode({k: str(v) for k, v in payload.items()})
        try:
            with urllib.request.urlopen(url.split("?")[0] + "?" + qs, timeout=2.5) as resp:
                return 200 <= getattr(resp, "status", 200) < 300
        except (urllib.error.URLError, TimeoutError, OSError):
            return False


def main() -> int:
    parser = argparse.ArgumentParser(description="Trello → ESP32")
    parser.add_argument("--url", default=None)
    parser.add_argument("--once", action="store_true")
    parser.add_argument("--interval", type=int, default=180)
    parser.add_argument("--demo", action="store_true", help="Datos fake sin API")
    args = parser.parse_args()
    url = args.url or load_buddy_url()
    print(f"Trello poller → {url}")

    while True:
        try:
            if args.demo:
                data = {
                    "np": 2,
                    "nd": 2,
                    "no": 1,
                    "progress": [
                        {"title": "Ship buddy LCD", "due": "today", "overdue": False},
                        {"title": "Fix WiFi reconnect", "due": "", "overdue": False},
                    ],
                    "due": [
                        {"title": "Pay invoice", "due": "2d late", "overdue": True},
                        {"title": "Review PR", "due": "tomorrow", "overdue": False},
                    ],
                }
            else:
                cfg = load_trello_cfg()
                data = collect(cfg)
            ok = post_board(url, data)
            print(
                f"progress={data['np']} due={data['nd']} overdue={data['no']} "
                f"board={'ok' if ok else 'FAIL'}"
            )
        except Exception as e:
            print(f"error: {e}", file=sys.stderr)
        if args.once:
            return 0
        time.sleep(max(60, args.interval))


if __name__ == "__main__":
    raise SystemExit(main())
