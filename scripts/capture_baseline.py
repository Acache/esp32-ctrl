#!/usr/bin/env python3
"""Capture the Desk-Mate parent's effective working tree as baseline B1.

Read-only against the parent. Produces:
  integration/baseline/source/          immutable effective source snapshot
  integration/baseline/MANIFEST.json    per-file SHA-256/size/mode
  integration/baseline/parent-status.txt / git-diff.patch
  docs/evidence/desk-mate-baseline.json machine-readable provenance

Run from the feature workspace. Re-running without --force refuses to
overwrite an existing B1 because B1 is immutable by contract.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PARENT = (ROOT / ".." / "desk-mate").resolve()
BASELINE = ROOT / "integration" / "baseline"
SOURCE = BASELINE / "source"
EVIDENCE = ROOT / "docs" / "evidence"
SCHEMA_VERSION = 1

# Directories that constitute the effective Desk-Mate source for this feature.
# Nested third-party checkouts (_lv_sim, _waveshare_examples) are toolchain
# inputs; they are recorded by identity but not copied into B1.
COPY_ITEMS = [
    "firmware/desk-mate-app",
    "firmware/README.md",
    "tests",
    "tools/lv_sim",
    "docs",
    "spec",
    "log",
    "README.md",
    ".gitignore",
]
IGNORE_DIR_NAMES = {"build", "cmake-build-debug", "cmake-build-release", "__pycache__", ".pytest_cache", ".DS_Store"}
IGNORE_SUFFIXES = {".o", ".d", ".pyc", ".bmp", ".raw"}


def run_git(*args: str) -> str:
    return subprocess.run(
        ["git", "-C", str(PARENT), *args], check=True, capture_output=True, text=True
    ).stdout


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as fh:
        for chunk in iter(lambda: fh.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def ignored(rel: str) -> bool:
    parts = Path(rel).parts
    if any(part in IGNORE_DIR_NAMES for part in parts):
        return True
    return Path(rel).suffix.lower() in IGNORE_SUFFIXES


def iter_files(base: Path, rel_dir: str):
    target = base / rel_dir
    if not target.exists():
        return
    if target.is_file():
        yield target
        return
    for dirpath, dirnames, filenames in os.walk(target):
        dirnames[:] = sorted(d for d in dirnames if d not in IGNORE_DIR_NAMES)
        for name in sorted(filenames):
            path = Path(dirpath) / name
            rel = path.relative_to(base).as_posix()
            if name == ".DS_Store" or ignored(rel):
                continue
            yield path


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--force", action="store_true", help="replace a previously captured B1 (forbidden in normal delivery)")
    args = ap.parse_args()

    if not (PARENT / ".git").exists() or not (PARENT / "firmware/desk-mate-app/desk-mate-app.ino").exists():
        print(f"FATAL: parent Desk-Mate source not found at {PARENT}", file=sys.stderr)
        return 2
    if SOURCE.exists() and any(SOURCE.iterdir()) and not args.force:
        print(f"FATAL: {SOURCE} already exists. Baseline B1 is immutable; use --force only for a deliberate new baseline.", file=sys.stderr)
        return 3

    BASELINE.mkdir(parents=True, exist_ok=True)
    EVIDENCE.mkdir(parents=True, exist_ok=True)
    if SOURCE.exists():
        shutil.rmtree(SOURCE)
    SOURCE.mkdir(parents=True)

    head = run_git("rev-parse", "HEAD").strip()
    branch = run_git("branch", "--show-current").strip()
    status_text = run_git("status", "--porcelain=v2", "--branch")
    diff_text = run_git("diff", "--binary", "--no-color")
    staged_text = run_git("diff", "--cached", "--binary", "--no-color")
    (BASELINE / "parent-status.txt").write_text(status_text, encoding="utf-8")
    (BASELINE / "git-diff.patch").write_text(diff_text, encoding="utf-8")
    (BASELINE / "git-diff-staged.patch").write_text(staged_text, encoding="utf-8")

    # Copy effective source exactly as present on disk (HEAD + dirty + untracked).
    files = []
    for item in COPY_ITEMS:
        for src in iter_files(PARENT, item):
            rel = src.relative_to(PARENT).as_posix()
            dst = SOURCE / rel
            dst.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(src, dst)
            files.append(
                {
                    "path": rel,
                    "sha256": sha256_file(dst),
                    "size": dst.stat().st_size,
                    "mode": oct(dst.stat().st_mode & 0o777),
                }
            )
    files.sort(key=lambda x: x["path"])

    tracked = run_git("ls-files", "-z").split("\0")
    tracked = [p for p in tracked if p and not ignored(p)]
    untracked = [ln.split("\t", 1)[-1] for ln in status_text.splitlines() if ln.startswith("? ")]
    dirty = []
    for ln in status_text.splitlines():
        if ln.startswith("1 "):
            fields = ln.split()
            dirty.append({"path": fields[8], "kind": "tracked-modified", "mode": fields[1]})
        elif ln.startswith("? "):
            dirty.append({"path": ln.split("\t", 1)[-1], "kind": "untracked"})

    manifest = {
        "schema_version": SCHEMA_VERSION,
        "baseline_id": "B1",
        "captured_at": datetime.now(timezone.utc).isoformat(),
        "parent_repo": str(PARENT),
        "head": head,
        "branch": branch,
        "status_porcelain_v2": status_text,
        "dirty_entries": dirty,
        "tracked_file_count": len(tracked),
        "copied_file_count": len(files),
        "copy_items": COPY_ITEMS,
        "excludes": sorted(IGNORE_DIR_NAMES | IGNORE_SUFFIXES),
        "files": files,
        "untracked": untracked,
        "snapshot_path": str(SOURCE),
        "notes": [
            "Snapshot is the effective parent working tree (HEAD + staged + unstaged + relevant untracked), not HEAD alone.",
            "Parent remains read-only; this script only read from it.",
            "Nested checkouts _lv_sim and _waveshare_examples are tool inputs and are not part of B1.",
        ],
    }
    (BASELINE / "MANIFEST.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2), encoding="utf-8")

    evidence = {
        "schema_version": SCHEMA_VERSION,
        "baseline_id": "B1",
        "captured_at": manifest["captured_at"],
        "parent_repo": str(PARENT),
        "parent_head": head,
        "parent_branch": branch,
        "parent_status": status_text,
        "manifest": "integration/baseline/MANIFEST.json",
        "snapshot": "integration/baseline/source/",
        "diff": "integration/baseline/git-diff.patch",
        "staged_diff": "integration/baseline/git-diff-staged.patch",
        "dirty_entries": dirty,
        "key_hashes": {
            f["path"]: f["sha256"]
            for f in files
            if f["path"]
            in {
                "firmware/desk-mate-app/desk-mate-app.ino",
                "firmware/desk-mate-app/focus_ui.h",
                "firmware/desk-mate-app/focus_model.h",
                "firmware/desk-mate-app/media_ble.h",
                "firmware/desk-mate-app/user_config.h",
                "firmware/desk-mate-app/lv_conf.h",
                "firmware/desk-mate-app/ui_pages.h",
                "tests/focus_model_test.c",
            }
        },
        "toolchain_inputs": {
            "_lv_sim": {"path": str(PARENT / "_lv_sim"), "role": "LVGL 9 simulator dependency (nested checkout, not B1)"},
            "_waveshare_examples": {"path": str(PARENT / "_waveshare_examples"), "role": "vendor examples, LVGL 8 and codec sources (nested checkout, not B1)"},
        },
    }
    (EVIDENCE / "desk-mate-baseline.json").write_text(
        json.dumps(evidence, ensure_ascii=False, indent=2), encoding="utf-8"
    )
    print(f"captured B1: HEAD={head} files={len(files)} -> {SOURCE}")
    print(f"manifest: {BASELINE / 'MANIFEST.json'}")
    print(f"evidence: {EVIDENCE / 'desk-mate-baseline.json'}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
