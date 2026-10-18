#!/usr/bin/env python3
"""Ask for Wi-Fi credentials with native macOS dialogs; write a 0600 local file.

The password never appears on the command line, in stdout, or in this repo.
"""
from __future__ import annotations

import json
import os
import subprocess
import sys
from pathlib import Path

DEFAULT_SSID = os.environ.get("ESP32CTRL_WIFI_SSID", "")
OUT = Path("/tmp/va-wifi.json")


def osa(script: str) -> str:
    result = subprocess.run(["osascript", "-e", script], capture_output=True, text=True, timeout=300)
    if result.returncode != 0:
        raise RuntimeError(result.stderr.strip() or "dialog cancelled")
    return result.stdout.strip()


def ask_text(prompt: str, default: str = "") -> str:
    script = (
        'display dialog ' + json.dumps(prompt, ensure_ascii=False) +
        ' default answer ' + json.dumps(default, ensure_ascii=False) +
        ' with title "Desk-Mate Wi-Fi" buttons {"Cancel","OK"} default button "OK"'
    )
    out = osa(script)
    # AppleScript result: button returned:OK, text returned:<value>
    marker = "text returned:"
    return out.split(marker, 1)[1] if marker in out else ""


def ask_hidden(prompt: str) -> str:
    script = (
        'display dialog ' + json.dumps(prompt, ensure_ascii=False) +
        ' default answer "" with hidden answer with title "Desk-Mate Wi-Fi" '
        'buttons {"Cancel","OK"} default button "OK"'
    )
    out = osa(script)
    marker = "text returned:"
    return out.split(marker, 1)[1] if marker in out else ""


def main() -> int:
    try:
        ssid = ask_text("Main 2.4 GHz Wi-Fi SSID (not the guest _G)", DEFAULT_SSID)
        if not ssid:
            print("no SSID", file=sys.stderr)
            return 2
        password = ask_hidden(f"Wi-Fi password for {ssid}")
        if not password:
            print("no password", file=sys.stderr)
            return 3
        OUT.write_text(json.dumps({"ssid": ssid, "password": password}))
        os.chmod(OUT, 0o600)
        print(f"wrote {OUT} (mode 0600, ssid={ssid}, password_length={len(password)})")
        return 0
    except Exception as exc:
        print(f"wifi prompt failed: {exc}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
