"""Batch higher-order analysis of explicit feedback groups.

The model is advisory only. It never edits rules or trains/deploys a model;
its report is stored for human review and regression corpus construction.
"""
from __future__ import annotations

import json
import os
import time
import uuid
from pathlib import Path
from typing import Any

import requests

from .config import AppConfig
from .storage import Database, utc_now

BATCH_SIZE = 50
MIN_EXPLICIT_LABELS = 10
SYSTEM_PROMPT = """You analyze a sparse batch of voice-assistant command feedback.
Each group has the user's transcript, normalized text, plan, execution evidence,
screen context, and GOOD/BAD/UNFEEDBACK. UNFEEDBACK is NOT a success label.
Return JSON only with keys: clusters, likely_root_causes, suggested_rules,
suggested_hotwords, suggested_tests, risks. Be conservative; never claim that
an unverified execution succeeded."""


def _explicit(row: dict[str, Any]) -> bool:
    return row.get("rating") in ("good", "bad")


def batch(db: Database, limit: int = BATCH_SIZE) -> dict[str, Any]:
    rows = db.feedback_groups(limit)
    explicit = [r for r in rows if _explicit(r)]
    compact = []
    for r in rows:
        try:
            context = json.loads(r.get("context_json") or "{}")
        except Exception:
            context = {}
        # Keep the model input bounded and avoid dumping full trees.
        steps = context.get("steps") or []
        compact.append({
            "command_id": r.get("id"),
            "text": r.get("normalized_text"),
            "raw_text": r.get("raw_text"),
            "plan": r.get("plan_json"),
            "state": r.get("state"),
            "summary": r.get("summary"),
            "error_code": r.get("error_code"),
            "rating": r.get("rating"),
            "dismissed_by": r.get("dismissed_by"),
            "touch_count": r.get("touch_count"),
            "context": {
                "page": context.get("page"),
                "steps": steps[:4],
                "player": context.get("player"),
                "search_query": (context.get("search") or {}).get("query"),
            },
        })
    return {
        "groups": compact,
        "total": len(compact),
        "explicit": len(explicit),
        "bad": sum(1 for r in explicit if r.get("rating") == "bad"),
        "good": sum(1 for r in explicit if r.get("rating") == "good"),
    }


def _call_model(cfg: AppConfig, payload: dict[str, Any]) -> tuple[str, str, dict[str, Any] | None]:
    prompt = SYSTEM_PROMPT + "\n\nBATCH JSON:\n" + json.dumps(payload, ensure_ascii=False)
    api_url = os.environ.get("VOICE_ANALYSIS_API_URL", "").strip()
    api_key = (os.environ.get("VOICE_ANALYSIS_API_KEY", "").strip()
               or os.environ.get("DEEPSEEK_API_KEY", "").strip())
    model = os.environ.get("VOICE_ANALYSIS_MODEL", "").strip()
    # DeepSeek official OpenAI-compatible endpoint; user may override.  Text
    # analysis only; image/vision is not needed or sent here.
    if not api_url and os.environ.get("DEEPSEEK_API_KEY", "").strip():
        api_url = "https://api.deepseek.com/chat/completions"
    if api_url and not model:
        model = "deepseek-flash" if "deepseek" in api_url else "gpt-4o-mini"
    if api_url:
        # OpenAI-compatible chat endpoint; caller opted in by setting env.
        body = {
            "model": model or "gpt-4o-mini",
            "messages": [
                {"role": "system", "content": SYSTEM_PROMPT},
                {"role": "user", "content": json.dumps(payload, ensure_ascii=False)},
            ],
            "temperature": 0,
            "response_format": {"type": "json_object"},
        }
        headers = {"Content-Type": "application/json"}
        if api_key:
            headers["Authorization"] = f"Bearer {api_key}"
        r = requests.post(api_url, json=body, headers=headers, timeout=180)
        r.raise_for_status()
        data = r.json()
        text = data["choices"][0]["message"]["content"]
        return "online", model or "gpt-4o-mini", json.loads(text)
    model = model or "qwen2.5:7b"
    r = requests.post(
        cfg.ollama_url.rstrip("/") + "/api/generate",
        json={"model": model, "prompt": prompt, "stream": False, "format": "json"},
        timeout=300,
    )
    r.raise_for_status()
    text = r.json().get("response", "")
    return "local", model, json.loads(text)


def analyze(cfg: AppConfig, db: Database, *, limit: int = BATCH_SIZE,
            min_labels: int = MIN_EXPLICIT_LABELS) -> dict[str, Any]:
    data = batch(db, limit)
    now = utc_now()
    run_id = str(uuid.uuid4())
    report: dict[str, Any]
    status = "insufficient"
    if data["total"] < limit or data["explicit"] < min_labels:
        report = {
            "status": "insufficient",
            "reason": f"need {limit} groups and {min_labels} explicit labels",
            **{k: data[k] for k in ("total", "explicit", "good", "bad")},
        }
    else:
        try:
            source, model, model_json = _call_model(cfg, data)
            report = {"status": "ok", "source": source, "model": model, "analysis": model_json,
                      "groups": data["total"], "explicit": data["explicit"]}
            status = "ok"
        except Exception as exc:
            report = {"status": "model_unavailable", "error": str(exc)[:300],
                      "groups": data["total"], "explicit": data["explicit"]}
            status = "model_unavailable"
    out_dir = cfg.root / "feedback"
    out_dir.mkdir(parents=True, exist_ok=True, mode=0o700)
    path = out_dir / f"analysis-{int(now)}-{run_id[:8]}.json"
    path.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    with db.transaction() as tx:
        tx.execute(
            "INSERT INTO feedback_analysis_runs(id,created_at,groups,explicit_labels,model,report_json,status) "
            "VALUES(?,?,?,?,?,?,?)",
            (run_id, now, data["total"], data["explicit"],
             report.get("model"), json.dumps(report, ensure_ascii=False), status),
        )
    return {"run_id": run_id, "report_path": str(path), **report}


def due(db: Database, limit: int = BATCH_SIZE, min_labels: int = MIN_EXPLICIT_LABELS) -> bool:
    with db._lock:
        counts = db._conn.execute(
            "SELECT COUNT(*) AS total, "
            "SUM(CASE WHEN rating IN ('good','bad') THEN 1 ELSE 0 END) AS explicit "
            "FROM feedback"
        ).fetchone()
        last_row = db._conn.execute("SELECT MAX(groups) AS g FROM feedback_analysis_runs").fetchone()
    total = int(counts["total"] or 0)
    explicit = int(counts["explicit"] or 0)
    last = int(last_row["g"] or 0)
    return total >= last + limit and explicit >= min_labels
