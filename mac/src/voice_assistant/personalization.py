"""Small, local, opt-in intent personalization.

This is the first stage of voice personalization, not ASR fine-tuning: train a
character n-gram intent classifier from the user's own normalized transcripts
and accepted plans. It is intentionally conservative and only predicts the
parameterless R1 actions open/pause/resume; search/play still require explicit
queries/targets. Training is offline; the online service never trains itself.
"""
from __future__ import annotations

import hashlib
import json
import time
from pathlib import Path
from typing import Any

from .config import AppConfig
from .contracts import DecisionProposal, DecisionStatus, ReasonCode
from .intent import _ok
from .storage import Database

MODEL_VERSION = 1
SUPPORTED_LABELS = {"bilibili.open", "bilibili.pause", "bilibili.resume"}
MIN_SAMPLES = 12
CONFIDENCE = 0.55


def _paths(cfg: AppConfig) -> tuple[Path, Path]:
    root = cfg.root / "personalization"
    return root / "intent-model.joblib", root / "intent-model.json"


def _examples(db: Database) -> tuple[list[str], list[str]]:
    texts: list[str] = []
    labels: list[str] = []
    with db._lock:
        rows = db._conn.execute(
            "SELECT normalized_text, plan_json FROM commands "
            "WHERE normalized_text IS NOT NULL AND plan_json IS NOT NULL"
        ).fetchall()
    for row in rows:
        text = str(row["normalized_text"] or "").strip()
        if not text:
            continue
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
        texts.append(text)
        labels.append(action)
    return texts, labels


def train(cfg: AppConfig, db: Database) -> dict[str, Any]:
    from sklearn.feature_extraction.text import TfidfVectorizer
    from sklearn.linear_model import LogisticRegression
    from sklearn.metrics import accuracy_score
    from sklearn.model_selection import train_test_split
    from sklearn.pipeline import Pipeline
    import joblib

    texts, labels = _examples(db)
    if len(texts) < MIN_SAMPLES:
        raise ValueError(f"not enough labeled samples: {len(texts)} < {MIN_SAMPLES}")
    if len(set(labels)) < 2:
        raise ValueError("need at least two intent labels")
    # Group by exact/near-duplicate text so the holdout is not the same phrase.
    train_x, test_x, train_y, test_y = train_test_split(
        texts, labels, test_size=0.25, random_state=42, stratify=labels
    )
    model = Pipeline([
        ("tfidf", TfidfVectorizer(analyzer="char_wb", ngram_range=(2, 4), min_df=1)),
        ("clf", LogisticRegression(max_iter=1000, class_weight="balanced")),
    ])
    model.fit(train_x, train_y)
    predicted = model.predict(test_x)
    accuracy = float(accuracy_score(test_y, predicted))
    dataset_hash = hashlib.sha256("\n".join(f"{t}\t{l}" for t, l in zip(texts, labels)).encode()).hexdigest()
    model_path, meta_path = _paths(cfg)
    model_path.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    joblib.dump(model, model_path)
    meta = {
        "schema_version": MODEL_VERSION,
        "trained_at": time.time(),
        "samples": len(texts),
        "labels": sorted(set(labels)),
        "accuracy_holdout": round(accuracy, 4),
        "dataset_hash": dataset_hash,
        "model_path": str(model_path),
        "mode": "intent-char-ngram-logreg",
        "auto_deploy": False,
    }
    meta_path.write_text(json.dumps(meta, ensure_ascii=False, indent=2) + "\n")
    return meta


class Personalizer:
    def __init__(self, cfg: AppConfig):
        self.cfg = cfg
        self._model = None
        self._meta: dict[str, Any] | None = None

    def _load(self) -> bool:
        if self._model is not None:
            return True
        model_path, meta_path = _paths(self.cfg)
        if not model_path.is_file() or not meta_path.is_file():
            return False
        try:
            import joblib
            self._model = joblib.load(model_path)
            self._meta = json.loads(meta_path.read_text())
            return True
        except Exception:
            self._model = None
            self._meta = None
            return False

    def propose(self, text: str) -> DecisionProposal | None:
        if not text or not self._load():
            return None
        try:
            proba = self._model.predict_proba([text])[0]
            best = max(range(len(proba)), key=lambda i: float(proba[i]))
            confidence = float(proba[best])
            label = str(self._model.classes_[best])
            if confidence < CONFIDENCE or label not in SUPPORTED_LABELS:
                return None
            return _ok([(label, {}, [])])
        except Exception:
            return None

    def info(self) -> dict[str, Any]:
        meta = self._meta
        if meta is None:
            _, meta_path = _paths(self.cfg)
            if meta_path.is_file():
                try:
                    meta = json.loads(meta_path.read_text())
                except Exception:
                    meta = None
        return meta or {"available": False}
