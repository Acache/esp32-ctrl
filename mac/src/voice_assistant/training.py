"""Local, opt-in training reminders.

The design intentionally never trains automatically. It reports when enough
reviewable command/audio samples exist, and stores the last reminder marker so
the user is nudged periodically (default: every 100 new samples).
"""
from __future__ import annotations

import json
import subprocess
from pathlib import Path
from typing import Any

from .storage import Database, utc_now

EARLY_REMINDER_SAMPLES = 100
TRAINING_REVIEW_SAMPLES = 200
CORRECTION_RATE_THRESHOLD = 0.15
REMINDER_PREF = "training.last_reminder_sample_count"


def status(db: Database) -> dict[str, Any]:
    with db._lock:
        samples = db._conn.execute(
            "SELECT COUNT(*) AS n FROM commands WHERE normalized_text IS NOT NULL"
        ).fetchone()["n"]
        failures = db._conn.execute(
            "SELECT COUNT(*) AS n FROM commands WHERE normalized_text IS NOT NULL "
            "AND state IN ('FAILED','OUTCOME_UNKNOWN','PARTIAL')"
        ).fetchone()["n"]
        corrections = db._conn.execute(
            "SELECT COUNT(*) AS n FROM feedback WHERE kind IN ('correction','redo','cancel_after')"
        ).fetchone()["n"]
        last = db._conn.execute(
            "SELECT value_json FROM preferences WHERE key=?", (REMINDER_PREF,)
        ).fetchone()
        pinned = db._conn.execute(
            "SELECT COUNT(*) AS n FROM audio_samples WHERE pinned=1"
        ).fetchone()["n"]
    last_count = 0
    if last:
        try:
            last_count = int(json.loads(last["value_json"]))
        except (TypeError, ValueError):
            last_count = 0
    correction_rate = (corrections / samples) if samples else 0.0
    failure_rate = (failures / samples) if samples else 0.0
    formal = samples >= TRAINING_REVIEW_SAMPLES or correction_rate > CORRECTION_RATE_THRESHOLD
    due = samples >= EARLY_REMINDER_SAMPLES and samples >= last_count + EARLY_REMINDER_SAMPLES
    return {
        "samples": samples,
        "corrections": corrections,
        "correction_rate": round(correction_rate, 4),
        "failure_rate": round(failure_rate, 4),
        "pinned_audio": pinned,
        "early_threshold": EARLY_REMINDER_SAMPLES,
        "training_review_threshold": TRAINING_REVIEW_SAMPLES,
        "correction_rate_threshold": CORRECTION_RATE_THRESHOLD,
        "last_reminder_sample_count": last_count,
        "reminder_due": due,
        "training_review_recommended": formal,
        "auto_train": False,
    }


def maybe_remind(db: Database, display_notification: bool = False) -> dict[str, Any]:
    result = status(db)
    # The cadence is intentionally "every 100 new samples"; formal review
    # recommendation is reported in the reminder text, not repeated forever.
    if not result["reminder_due"]:
        return result
    # Only advance the marker when this call actually surfaces a reminder.
    with db.transaction() as tx:
        tx.execute(
            "INSERT INTO preferences(key,value_json,source,updated_at) VALUES(?,?,?,?) "
            "ON CONFLICT(key) DO UPDATE SET value_json=excluded.value_json,"
            "source=excluded.source,updated_at=excluded.updated_at",
            (REMINDER_PREF, json.dumps(result["samples"]), "manual", utc_now()),
        )
    result["last_reminder_sample_count"] = result["samples"]
    result["reminder_due"] = False
    if display_notification:
        message = (
            f"已有 {result['samples']} 条语音样本"
            + ("，更正率 %.0f%%，建议安排离线训练评估" % (result["correction_rate"] * 100)
               if result["training_review_recommended"] else "，可开始准备个性化热词/训练数据")
        )
        try:
            subprocess.run(
                ["osascript", "-e", f'display notification {json.dumps(message)} with title "Desk-Mate 语音训练提醒"'],
                capture_output=True, text=True, timeout=5,
            )
        except Exception:
            pass
    return result


def export_dataset(cfg, db: Database, dest: Path) -> dict[str, Any]:
    """Export opt-in local training data with a manifest; never uploads."""
    import shutil

    dest = Path(dest)
    dest.mkdir(parents=True, exist_ok=False, mode=0o700)
    audio_dir = dest / "audio"
    audio_dir.mkdir(mode=0o700)
    samples = dest / "samples.jsonl"
    count = 0
    with db._lock:
        rows = db._conn.execute(
            "SELECT c.id, c.normalized_text, c.raw_text, c.plan_json, c.state, "
            "a.relative_path, a.retention_until, a.pinned "
            "FROM commands c LEFT JOIN audio_samples a ON a.command_id=c.id "
            "WHERE c.normalized_text IS NOT NULL ORDER BY c.created_at"
        ).fetchall()
    with samples.open("w", encoding="utf-8") as out:
        for row in rows:
            path = None
            if row["relative_path"]:
                src = cfg.root / row["relative_path"]
                if src.is_file():
                    dst = audio_dir / src.name
                    shutil.copy2(src, dst)
                    path = str(dst.relative_to(dest))
            record = {
                "command_id": row["id"],
                "normalized_text": row["normalized_text"],
                "raw_text": row["raw_text"],
                "plan_json": row["plan_json"],
                "state": row["state"],
                "audio": path,
                "retention_until": row["retention_until"],
                "pinned": bool(row["pinned"]),
            }
            out.write(json.dumps(record, ensure_ascii=False) + "\n")
            count += 1
    (dest / "MANIFEST.json").write_text(json.dumps({
        "samples": count,
        "created_at": utc_now(),
        "source": str(cfg.root),
        "local_only": True,
        "note": "Opt-in training export; audio retention 1825 days, pinned kept.",
    }, ensure_ascii=False, indent=2) + "\n")
    return {"dest": str(dest), "samples": count}
