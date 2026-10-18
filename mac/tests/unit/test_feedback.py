from __future__ import annotations

import json
import time
import uuid
from pathlib import Path

from voice_assistant import feedback_analysis
from voice_assistant.auto_training import readiness
from voice_assistant.storage import Database, utc_now


def _seed(db: Database, n_groups: int = 50, explicit: int = 10) -> tuple[str, list[str]]:
    device_id, _ = db.register_device("feedback-test")
    ids = []
    now = utc_now()
    with db.transaction() as tx:
        for i in range(n_groups):
            cid = str(uuid.uuid4())
            action = "bilibili.open" if i % 2 == 0 else "bilibili.pause"
            plan = json.dumps({"actions": [{"action": action, "after": [], "binding": {}, "params": {}}]})
            tx.execute(
                "INSERT INTO commands(id,device_id,kind,source,state,created_at,updated_at,"
                "committed_at,expires_at,normalized_text,raw_text,plan_json,summary) "
                "VALUES(?,?,?,?,?,?,?,?,?,?,?,?,?)",
                (cid, device_id, "audio", "esp32_ptt", "SUCCEEDED", now, now, now, now + 120,
                 f"命令{i}", f"命令{i}。", plan, "完成"),
            )
            ids.append(cid)
    for i, cid in enumerate(ids[:explicit]):
        db.record_feedback(
            cid, device_id, rating="good" if i % 2 == 0 else "bad",
            dismissed_by="selected", touch_count=1, countdown_at_dismiss=2,
            page=2, source="device",
        )
    for cid in ids[explicit:]:
        db.record_feedback(
            cid, device_id, rating="unfeedback", dismissed_by="timeout",
            touch_count=0, countdown_at_dismiss=0, page=2, source="server",
        )
    return device_id, ids


def test_feedback_batch_and_due_gate(tmp_path: Path, monkeypatch):
    db = Database(tmp_path / "learning.db")
    _seed(db)
    data = feedback_analysis.batch(db, 50)
    assert data["total"] == 50
    assert data["explicit"] == 10
    assert feedback_analysis.due(db) is True

    monkeypatch.setattr(
        feedback_analysis, "_call_model",
        lambda cfg, payload: ("test", "fake", {"clusters": [], "risks": []}),
    )
    from types import SimpleNamespace
    cfg = SimpleNamespace(root=tmp_path, ollama_url="http://127.0.0.1:11434")
    result = feedback_analysis.analyze(cfg, db, limit=50, min_labels=10)
    assert result["status"] == "ok"
    assert result["report_path"]
    assert feedback_analysis.due(db) is False
    db.close()


def test_feedback_readiness_and_precedence(tmp_path: Path):
    db = Database(tmp_path / "learning.db")
    device_id, ids = _seed(db)
    status = readiness(db)
    assert status["explicit_labels"] == 10
    assert status["ready"] is True

    cid = ids[0]
    db.record_feedback(cid, device_id, rating="unfeedback", dismissed_by="server_inferred",
                       touch_count=0, countdown_at_dismiss=0, source="server")
    db.record_feedback(cid, device_id, rating="good", dismissed_by="selected",
                       touch_count=1, countdown_at_dismiss=2, source="device")
    row = db.feedback_for_command(cid)
    assert row["rating"] == "good"
    assert row["source"] == "device"
    db.close()


def test_auto_training_validation_gate(tmp_path: Path):
    from types import SimpleNamespace
    from voice_assistant.auto_training import train_if_ready
    db = Database(tmp_path / "learning.db")
    _seed(db)
    cfg = SimpleNamespace(root=tmp_path)
    first = train_if_ready(cfg, db)
    assert first["trained"] is True
    assert first["deployed"] is True
    meta_path = tmp_path / "personalization" / "intent-model.json"
    meta = json.loads(meta_path.read_text())
    meta["accuracy_holdout"] = 1.0
    meta_path.write_text(json.dumps(meta))
    second = train_if_ready(cfg, db)
    assert second["trained"] is True
    assert second["deployed"] is False
    assert "did not improve" in second["reason"]
    db.close()
