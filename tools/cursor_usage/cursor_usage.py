#!/usr/bin/env python3
"""Poll Cursor Pro usage → ESP32 Buddy (POST /usage).

Lee el accessToken local de Cursor y llama GetCurrentPeriodUsage:
  autoPercentUsed → Cursor Models
  apiPercentUsed  → Other Models
"""

from __future__ import annotations

import argparse
import json
import os
import sqlite3
import sys
import time
import urllib.error
import urllib.parse
import urllib.request

CONFIG_PATH = os.path.expanduser("~/.cursor/esp32-buddy.json")
STATE_DB = os.path.expanduser(
    "~/Library/Application Support/Cursor/User/globalStorage/state.vscdb"
)


def load_board_url() -> str:
    try:
        with open(CONFIG_PATH, "r", encoding="utf-8") as f:
            cfg = json.load(f)
        base = cfg.get("url") or (cfg.get("urls") or [None])[0]
        if isinstance(base, str) and base:
            if base.endswith("/event"):
                return base[: -len("/event")] + "/usage"
            if base.endswith("/usage"):
                return base
            return base.rstrip("/") + "/usage"
    except (OSError, json.JSONDecodeError, TypeError, IndexError):
        pass
    return "http://cursor-buddy.local/usage"


def read_access_token() -> str:
    con = sqlite3.connect(f"file:{STATE_DB}?mode=ro", uri=True)
    try:
        row = con.execute(
            "SELECT value FROM ItemTable WHERE key='cursorAuth/accessToken'"
        ).fetchone()
    finally:
        con.close()
    if not row or not row[0]:
        raise RuntimeError("No cursorAuth/accessToken in Cursor DB (¿logueado?)")
    return str(row[0])


def fetch_usage(token: str) -> tuple[int, int]:
    req = urllib.request.Request(
        "https://api2.cursor.sh/aiserver.v1.DashboardService/GetCurrentPeriodUsage",
        data=b"{}",
        headers={
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/json",
            "Connect-Protocol-Version": "1",
            "User-Agent": "esp32-cursor-buddy/1.0",
        },
        method="POST",
    )
    with urllib.request.urlopen(req, timeout=20) as resp:
        data = json.loads(resp.read().decode())
    plan = data.get("planUsage") or {}
    cursor = int(round(float(plan.get("autoPercentUsed") or 0)))
    other = int(round(float(plan.get("apiPercentUsed") or 0)))
    return max(0, min(100, cursor)), max(0, min(100, other))


def post_usage(url: str, cursor: int, other: int) -> bool:
    payload = {"cursor": cursor, "other": other}
    data = json.dumps(payload).encode()
    req = urllib.request.Request(
        url, data=data, headers={"Content-Type": "application/json"}, method="POST"
    )
    try:
        with urllib.request.urlopen(req, timeout=2.0) as resp:
            return 200 <= getattr(resp, "status", 200) < 300
    except (urllib.error.URLError, TimeoutError, OSError):
        qs = urllib.parse.urlencode(payload)
        try:
            with urllib.request.urlopen(url.split("?")[0] + "?" + qs, timeout=2.0) as resp:
                return 200 <= getattr(resp, "status", 200) < 300
        except (urllib.error.URLError, TimeoutError, OSError):
            return False


def main() -> int:
    parser = argparse.ArgumentParser(description="Cursor usage → ESP32")
    parser.add_argument("--url", default=None)
    parser.add_argument("--once", action="store_true")
    parser.add_argument("--interval", type=int, default=120, help="segundos entre polls")
    args = parser.parse_args()
    url = args.url or load_board_url()
    print(f"Usage poller → {url} (every {args.interval}s)")

    while True:
        try:
            token = read_access_token()
            cursor, other = fetch_usage(token)
            ok = post_usage(url, cursor, other)
            print(f"cursor={cursor}% other={other}% board={'ok' if ok else 'FAIL'}")
        except Exception as e:
            print(f"error: {e}", file=sys.stderr)
        if args.once:
            return 0
        time.sleep(max(30, args.interval))


if __name__ == "__main__":
    raise SystemExit(main())
