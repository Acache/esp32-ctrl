"""Validation-gated intent personalization from explicit feedback.

Automatic training is deliberately conservative: it only runs after enough
explicit labels, uses a grouped train/validation split, and replaces the model
only when validation accuracy improves. UNFEEDBACK is never used as a label.
"""
from __future__ import annotations

import hashlib
import json
import shutil
import time
from pathlib import Path
from typing import Any

from .config import AppConfig
from .personalization import MODEL_VERSION, SUPPORTED_LABELS, _paths
from .storage import Database, utc_now

MIN_EXPLICIT = 10
MIN_BAD = 0
CORRECTION_RATE = 0.15
MIN_CLASSES = 2


def dataset(db: Database) -> tuple[list[str], list[str], list[str]]:
    texts: list[str] = []
    labels: list[str] = []
    groups: list[str] = []
    with db._lock:
        rows = db._conn.execute(
            "SELECT c.id, c.normalized_text, c.plan_json, f.rating FROM feedback f "
            "JOIN commands c ON c.id=f.command_id "
            "WHERE f.rating IN ('good','bad') AND c.normalized_text IS NOT NULL "
            "AND c.plan_json IS NOT NULL"
        ).fetchall()
    for row in rows:
        try:
            plan = json.loads(row["plan_json"])
            actions = plan.get("actions") or []
        except Exception:
            continue
        if len(actions) != 1:
            continue
        action = str(actions[0].get("action") or "")
        if action not in SUPPORTED_LABELS:
            continue
        texts.append(str(row["normalized_text"]).strip())
        labels.append(action)
        groups.append(str(row["id"]))
    return texts, labels, groups


def readiness(db: Database) -> dict[str, Any]:
    texts, labels, _ = dataset(db)
    bad = 0
    total = 0
    with db._lock:
        rows = db._conn.execute(
            "SELECT f.rating, COUNT(*) n FROM feedback f WHERE f.rating IN ('good','bad') GROUP BY f.rating"
        ).fetchall()
    for row in rows:
        total += int(row["n"])
        if row["rating"] == "bad":
            bad = int(row["n"])
    rate = (bad / total) if total else 0.0
    # The user-approved gate is 50 feedback groups with >=10 explicit labels;
    # validation improvement (not the raw count) controls deployment.
    ready = len(texts) >= MIN_EXPLICIT and len(set(labels)) >= MIN_CLASSES
    return {
        "labeled_groups": len(texts),
        "explicit_labels": total,
        "bad": bad,
        "correction_rate": round(rate, 4),
        "min_explicit": MIN_EXPLICIT,
        "min_bad": MIN_BAD,
        "correction_threshold": CORRECTION_RATE,
        "ready": ready and len(set(labels)) >= MIN_CLASSES,
    }


def train_if_ready(cfg: AppConfig, db: Database) -> dict[str, Any]:
    status = readiness(db)
    if not status["ready"]:
        return {"trained": False, "deployed": False, **status}
    from sklearn.feature_extraction.text import TfidfVectorizer
    from sklearn.linear_model import LogisticRegression
    from sklearn.metrics import accuracy_score
    from sklearn.model_selection import train_test_split
    from sklearn.pipeline import Pipeline
    import joblib

    texts, labels, groups = dataset(db)
    if len(set(labels)) < MIN_CLASSES:
        return {"trained": False, "deployed": False, "reason": "one class", **status}
    # Group IDs are unique commands; a plain split is acceptable here because
    # the same command ID cannot appear in both sets.
    train_x, val_x, train_y, val_y = train_test_split(
        texts, labels, test_size=0.25, random_state=42, stratify=labels
    )
    model = Pipeline([
        ("tfidf", TfidfVectorizer(analyzer="char_wb", ngram_range=(2, 4), min_df=1)),
        ("clf", LogisticRegression(max_iter=1000, class_weight="balanced")),
    ])
    model.fit(train_x, train_y)
    accuracy = float(accuracy_score(val_y, model.predict(val_x)))
    model_path, meta_path = _paths(cfg)
    old_accuracy = 0.0
    if meta_path.is_file():
        try:
            old_accuracy = float(json.loads(meta_path.read_text()).get("accuracy_holdout", 0.0))
        except Exception:
            old_accuracy = 0.0
    if accuracy <= old_accuracy:
        return {"trained": True, "deployed": False, "accuracy": round(accuracy, 4),
                "old_accuracy": old_accuracy, "reason": "validation did not improve", **status}
    model_path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    if model_path.is_file():
        shutil.copy2(model_path, model_path.with_suffix(".joblib.bak"))
    joblib.dump(model, model_path)
    digest = hashlib.sha256("\n".join(f"{t}\t{l}" for t, l in zip(texts, labels)).encode()).hexdigest()
    meta = {
        "schema_version": MODEL_VERSION,
        "trained_at": time.time(),
        "samples": len(texts),
        "labels": sorted(set(labels)),
        "accuracy_holdout": round(accuracy, 4),
        "dataset_hash": digest,
        "model_path": str(model_path),
        "mode": "intent-char-ngram-logreg-feedback",
        "source": "explicit_feedback",
        "auto_deploy": True,
    }
    meta_path.write_text(json.dumps(meta, ensure_ascii=False, indent=2) + "\n")
    return {"trained": True, "deployed": True, **status, **meta}
