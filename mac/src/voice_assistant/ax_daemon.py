"""Persistent macOS Accessibility helper for the Bilibili player.

The one-shot helper re-walks a large Chromium AX tree on every call (~1s).
This wrapper keeps one `ax_helper serve` process alive and serializes
JSON-line snapshot/toggle requests; requests are typically <100 ms plus the
intentional state-sampling wait inside the helper.
"""
from __future__ import annotations

import json
import queue
import subprocess
import threading
from typing import Any


class AxDaemonError(RuntimeError):
    pass


class AxDaemon:
    def __init__(self, helper: str) -> None:
        self.helper = helper
        self._proc: subprocess.Popen[str] | None = None
        self._lock = threading.Lock()
        self._lines: queue.Queue[str] = queue.Queue()
        self._reader: threading.Thread | None = None

    def _start_locked(self) -> None:
        if self._proc is not None and self._proc.poll() is None:
            return
        self._close_locked()
        self._lines = queue.Queue()
        self._proc = subprocess.Popen(
            [self.helper, "serve"],
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
            text=True, bufsize=1,
        )

        def reader() -> None:
            proc = self._proc
            if proc is None or proc.stdout is None:
                return
            for line in proc.stdout:
                self._lines.put(line)

        self._reader = threading.Thread(target=reader, name="ax-daemon-reader", daemon=True)
        self._reader.start()
        ready = self._read_locked(timeout=8.0)
        if not ready or not ready.get("ready"):
            self._close_locked()
            raise AxDaemonError("AX daemon did not become ready")

    def _read_locked(self, timeout: float) -> dict[str, Any] | None:
        try:
            raw = self._lines.get(timeout=timeout).strip()
        except queue.Empty:
            return None
        try:
            value = json.loads(raw)
        except json.JSONDecodeError:
            return None
        return value if isinstance(value, dict) else None

    def start(self) -> None:
        with self._lock:
            self._start_locked()

    def request(self, command: str, timeout: float = 6.0) -> dict[str, Any]:
        with self._lock:
            self._start_locked()
            assert self._proc is not None and self._proc.stdin is not None
            try:
                self._proc.stdin.write(command + "\n")
                self._proc.stdin.flush()
            except (BrokenPipeError, OSError) as exc:
                self._close_locked()
                raise AxDaemonError("AX daemon pipe closed") from exc
            result = self._read_locked(timeout)
            if result is None:
                self._close_locked()
                raise AxDaemonError("AX daemon timed out")
            return result

    def _close_locked(self) -> None:
        proc = self._proc
        self._proc = None
        if proc is None:
            return
        try:
            if proc.stdin:
                proc.stdin.write("quit\n")
                proc.stdin.flush()
        except (BrokenPipeError, OSError):
            pass
        try:
            proc.wait(timeout=1.0)
        except subprocess.TimeoutExpired:
            proc.kill()
        self._reader = None

    def close(self) -> None:
        with self._lock:
            self._close_locked()
