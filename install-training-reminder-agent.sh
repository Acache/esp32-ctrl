#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "$0")" && pwd)"
PLIST="$HOME/Library/LaunchAgents/com.desk-mate.training-reminder.plist"
mkdir -p "$(dirname "$PLIST")"
cat > "$PLIST" <<PL
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>Label</key><string>com.desk-mate.training-reminder</string>
  <key>ProgramArguments</key><array>
    <string>/usr/bin/env</string>
    <string>PYTHONPATH=$ROOT/mac/src</string>
    <string>$ROOT/.venv/bin/python</string>
    <string>-m</string>
    <string>voice_assistant.cli</string>
    <string>training-status</string>
    <string>--notify</string>
  </array>
  <key>StartInterval</key><integer>21600</integer>
  <key>RunAtLoad</key><false/>
  <key>StandardOutPath</key><string>$HOME/Library/Logs/desk-mate-training.log</string>
  <key>StandardErrorPath</key><string>$HOME/Library/Logs/desk-mate-training.err.log</string>
</dict></plist>
PL
launchctl unload "$PLIST" 2>/dev/null || true
launchctl load "$PLIST"
echo "installed $PLIST"
