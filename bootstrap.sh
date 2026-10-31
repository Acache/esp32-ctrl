#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
python3.11 -m venv .venv
.venv/bin/python -m pip install --upgrade pip
if [ -f mac/requirements.lock ]; then
  .venv/bin/pip install -r mac/requirements.lock
else
  echo "No requirements lock; install project dependencies manually." >&2
fi
PYTHONPATH=mac/src .venv/bin/python -m voice_assistant.cli setup
