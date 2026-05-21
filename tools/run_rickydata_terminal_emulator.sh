#!/usr/bin/env bash
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

BRIDGE_PORT="${RICKYDATA_BRIDGE_PORT:-8765}"
if ! curl -fsS "http://127.0.0.1:${BRIDGE_PORT}/health" >/dev/null 2>&1; then
  node tools/rickydata_terminal_bridge.mjs &
  BRIDGE_PID=$!
  trap 'kill "$BRIDGE_PID" 2>/dev/null || true' EXIT
  sleep 1
fi

pio run -e emulator_Core2
RICKYDATA_EMULATOR_SCREEN="${RICKYDATA_EMULATOR_SCREEN:-voice}" \
  ./.pio/build/emulator_Core2/program
