#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
VENV="$ROOT/tools/.venv"
if [[ ! -x "$VENV/bin/python" ]]; then
  python3 -m venv "$VENV"
  "$VENV/bin/pip" install -q -r "$ROOT/tools/requirements.txt"
fi
exec "$VENV/bin/python" "$ROOT/tools/keyboard_presence.py" "$@"
