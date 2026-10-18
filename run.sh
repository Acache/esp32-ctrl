#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT"
exec env PYTHONPATH="$ROOT/mac/src" "$ROOT/.venv/bin/python" -m voice_assistant.cli run "$@"
