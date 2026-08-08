#!/usr/bin/env python3
"""Keyboard Presence → ESP32-S3 Lab.

Cuenta teclas (sin guardar texto) y manda métricas a POST /keys.

Uso:
  python3 tools/keyboard_presence.py
  python3 tools/keyboard_presence.py --url http://192.168.0.15/keys

macOS: System Settings → Privacy & Security → Accessibility → permitir Terminal/Python.
"""

from __future__ import annotations

import argparse
import json
import os
import sys
import threading
import time
import urllib.error
import urllib.parse
import urllib.request
from collections import deque
from datetime import date, datetime, timedelta

CONFIG_PATH = os.path.expanduser("~/.cursor/esp32-buddy.json")
STATE_PATH = os.path.expanduser("~/.cursor/keyboard-presence.json")


def load_url(cli_url: str | None) -> str:
    if cli_url:
        return cli_url
    try:
        with open(CONFIG_PATH, "r", encoding="utf-8") as f:
            cfg = json.load(f)
        base = cfg.get("url") or (cfg.get("urls") or [None])[0]
        if isinstance(base, str) and base:
            if base.endswith("/event"):
                return base[: -len("/event")] + "/keys"
            if base.endswith("/keys"):
                return base
            return base.rstrip("/") + "/keys"
    except (OSError, json.JSONDecodeError, TypeError, IndexError):
        pass
    return "http://cursor-buddy.local/keys"


def load_state() -> dict:
    try:
        with open(STATE_PATH, "r", encoding="utf-8") as f:
            return json.load(f)
    except (OSError, json.JSONDecodeError):
        return {}


def save_state(state: dict) -> None:
    os.makedirs(os.path.dirname(STATE_PATH), exist_ok=True)
    with open(STATE_PATH, "w", encoding="utf-8") as f:
        json.dump(state, f, indent=2)


class PresenceTracker:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._pending = 0
        self._press_times: deque[float] = deque()
        self._last_key = 0.0
        self._by_day: dict[str, int] = {}
        self._streak = 0
        state = load_state()
        if isinstance(state.get("by_day"), dict):
            self._by_day = {str(k): int(v) for k, v in state["by_day"].items()}
        self._streak = int(state.get("streak") or 0)
        self._roll_day()

    def _roll_day(self) -> None:
        today = date.today().isoformat()
        self._by_day.setdefault(today, 0)
        # limpiar > 21 días
        cutoff = (date.today() - timedelta(days=21)).isoformat()
        self._by_day = {k: v for k, v in self._by_day.items() if k >= cutoff}
        self._recompute_streak()

    def _recompute_streak(self) -> None:
        streak = 0
        d = date.today()
        while True:
            key = d.isoformat()
            if self._by_day.get(key, 0) > 0:
                streak += 1
                d -= timedelta(days=1)
            else:
                break
        self._streak = streak

    def on_key(self) -> None:
        now = time.time()
        with self._lock:
            self._roll_day()
            today = date.today().isoformat()
            self._by_day[today] = self._by_day.get(today, 0) + 1
            self._pending += 1
            self._last_key = now
            self._press_times.append(now)
            while self._press_times and now - self._press_times[0] > 12.0:
                self._press_times.popleft()
            self._recompute_streak()

    def take_pending(self) -> int:
        with self._lock:
            n = self._pending
            self._pending = 0
            return n

    def snapshot(self) -> dict:
        with self._lock:
            self._roll_day()
            today = date.today()
            week = []
            for i in range(6, -1, -1):
                day = today - timedelta(days=i)
                week.append(int(self._by_day.get(day.isoformat(), 0)))
            now = time.time()
            while self._press_times and now - self._press_times[0] > 12.0:
                self._press_times.popleft()
            keys_12s = len(self._press_times)
            wpm = (keys_12s * 5.0) / 5.0  # keys/12s → keys/min / 5
            # keys in 12s * (60/12) / 5 = keys_12s * 1
            wpm = float(keys_12s)  # approx WPM if ~5 chars/word burst
            idle_s = int(now - self._last_key) if self._last_key else int(now)
            if self._last_key and now - self._last_key < 1.0:
                idle_s = 0
            payload = {
                "keys": int(self._by_day.get(today.isoformat(), 0)),
                "week": ",".join(str(x) for x in week),
                "streak": int(self._streak),
                "wpm": round(wpm, 1),
                "idle_s": max(0, idle_s),
            }
            save_state({"by_day": self._by_day, "streak": self._streak, "updated": datetime.now().isoformat()})
            return payload

    def live_meta(self) -> tuple[float, int]:
        with self._lock:
            now = time.time()
            while self._press_times and now - self._press_times[0] > 12.0:
                self._press_times.popleft()
            wpm = float(len(self._press_times))
            idle_s = int(now - self._last_key) if self._last_key else 999
            if self._last_key and now - self._last_key < 1.0:
                idle_s = 0
            return wpm, max(0, idle_s)


def post_json(url: str, payload: dict) -> bool:
    data = json.dumps(payload).encode("utf-8")
    req = urllib.request.Request(
        url, data=data, headers={"Content-Type": "application/json"}, method="POST"
    )
    try:
        with urllib.request.urlopen(req, timeout=1.2) as resp:
            return 200 <= getattr(resp, "status", 200) < 300
    except (urllib.error.URLError, TimeoutError, OSError):
        qs = urllib.parse.urlencode(payload)
        get_url = url.split("?")[0] + "?" + qs
        try:
            with urllib.request.urlopen(get_url, timeout=1.2) as resp:
                return 200 <= getattr(resp, "status", 200) < 300
        except (urllib.error.URLError, TimeoutError, OSError):
            return False


def main() -> int:
    parser = argparse.ArgumentParser(description="Keyboard Presence → ESP32")
    parser.add_argument("--url", default=None, help="http://IP/keys")
    parser.add_argument("--demo", action="store_true", help="Simula teclas sin listener")
    args = parser.parse_args()
    url = load_url(args.url)
    tracker = PresenceTracker()

    print(f"Keyboard Presence → {url}")
    print(f"State: {STATE_PATH}")

    if args.demo:
        print("Demo mode: enviando teclas simuladas. Ctrl+C para salir.")
        try:
            while True:
                for _ in range(5):
                    tracker.on_key()
                delta = tracker.take_pending()
                wpm, idle = tracker.live_meta()
                post_json(url, {"delta": delta, "wpm": wpm, "idle_s": idle})
                if int(time.time()) % 10 < 2:
                    post_json(url, tracker.snapshot())
                time.sleep(1.0)
        except KeyboardInterrupt:
            print("\nbye")
        return 0

    try:
        from pynput import keyboard as kb
    except ImportError:
        print("Falta pynput. Instalá con: pip3 install pynput", file=sys.stderr)
        print("O probá: python3 tools/keyboard_presence.py --demo", file=sys.stderr)
        return 1

    def on_press(_key):
        tracker.on_key()

    listener = kb.Listener(on_press=on_press)
    listener.start()
    print("Escuchando teclado (solo conteo). Ctrl+C para salir.")
    print("macOS: System Settings → Privacy & Security → Accessibility")
    print("       → habilitar Terminal / Python / Cursor.")

    last_snap = 0.0
    last_post = 0.0
    try:
        while listener.running:
            now = time.time()
            delta = tracker.take_pending()
            wpm, idle = tracker.live_meta()
            # Enviar pronto cuando hay teclas (tiempo real en la LCD)
            if delta > 0 or (idle < 30 and now - last_post >= 1.0):
                ok = post_json(url, {"delta": delta, "wpm": wpm, "idle_s": idle})
                last_post = now
                if not ok and delta:
                    print("warn: no llegó a la placa", file=sys.stderr)
            if now - last_snap >= 20:
                post_json(url, tracker.snapshot())
                last_snap = now
            time.sleep(0.08)
    except KeyboardInterrupt:
        print("\nbye")
        listener.stop()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
