"""Contract rejection and acceptance tests (red/green evidence for T1)."""
from __future__ import annotations

import json
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "src"))

from voice_assistant import contracts  # noqa: E402
from voice_assistant.contracts import (  # noqa: E402
    ContractError,
    DecisionProposal,
    StepState,
    canonical_json,
    decode_json,
    parse_client_message,
    validate_decision_proposal,
    validate_plan_structure,
)


def proposal(actions, status="ok", reason="none", **extra):
    return {
        "schema_version": 1,
        "status": status,
        "reason_code": reason,
        "clarification": extra.get("clarification"),
        "model_confidence": extra.get("model_confidence"),
        "actions": actions,
    }


def step(i, action, params=None, after=None):
    return {"id": i, "action": action, "params": params or {}, "after": after or []}


def test_unknown_field_rejected():
    msg = {
        "v": 1, "type": "begin", "msg_id": 1,
        "command_id": "11111111-1111-4111-8111-111111111111",
        "kind": "audio", "format": {"encoding": "s16le", "rate": 16000, "channels": 1},
        "trigger": "esp32_ptt", "extra": "nope",
    }
    with pytest.raises(ContractError):
        parse_client_message(json.dumps(msg))


def test_duplicate_json_key_rejected():
    raw = '{"v":1,"type":"ping","msg_id":1,"msg_id":2}'
    with pytest.raises(ContractError) as exc:
        decode_json(raw)
    assert "duplicate" in str(exc.value)


@pytest.mark.parametrize("raw", ['{"v":1,"type":"ping","msg_id":NaN}', '{"v":1,"type":"ping","msg_id":Infinity}'])
def test_non_finite_numbers_rejected(raw):
    with pytest.raises(ContractError):
        decode_json(raw)


def test_bool_as_int_rejected():
    msg = {
        "v": 1, "type": "seal", "msg_id": True,
        "command_id": "11111111-1111-4111-8111-111111111111",
        "frame_count": 1, "pcm_bytes": 640, "sha256": "0" * 64,
    }
    with pytest.raises(ContractError):
        parse_client_message(json.dumps(msg))


def test_invalid_uuid_rejected():
    msg = {"v": 1, "type": "status", "msg_id": 1, "command_id": "not-a-uuid"}
    with pytest.raises(ContractError):
        parse_client_message(json.dumps(msg))


def test_oversized_control_rejected():
    msg = {"v": 1, "type": "ping", "msg_id": 1, "pad": "x" * 3000}
    with pytest.raises(ContractError):
        decode_json(json.dumps(msg))


def test_invalid_utf8_rejected():
    with pytest.raises(ContractError):
        decode_json(b"\xff\xfe\x00")


def test_hello_success():
    msg = {
        "v": 1, "type": "hello", "msg_id": 1,
        "device_id": "11111111-1111-4111-8111-111111111111",
        "server_id": "22222222-2222-4222-8222-222222222222",
        "firmware_version": "desk-mate-bili/1.0",
        "boot_id": "33333333-3333-4333-8333-333333333333",
    }
    parsed = parse_client_message(msg)
    assert parsed.type == "hello"


def test_begin_format_fixed():
    bad = {
        "v": 1, "type": "begin", "msg_id": 1,
        "command_id": "11111111-1111-4111-8111-111111111111",
        "kind": "audio", "format": {"encoding": "s16le", "rate": 8000, "channels": 1},
        "trigger": "esp32_ptt",
    }
    with pytest.raises(ContractError):
        parse_client_message(json.dumps(bad))


def test_seal_hash_case_and_length():
    base = {
        "v": 1, "type": "seal", "msg_id": 1,
        "command_id": "11111111-1111-4111-8111-111111111111",
        "frame_count": 1, "pcm_bytes": 640, "sha256": "A" * 64,
    }
    with pytest.raises(ContractError):
        parse_client_message(json.dumps(base))


def test_play_three_mutually_exclusive_forms():
    bv = validate_decision_proposal(proposal([step(0, "bilibili.play", {"bv": "BV1LqJC6iEZV"})]))
    assert bv.actions[0].params["bv"] == "BV1LqJC6iEZV"
    idx = validate_decision_proposal(proposal([
        step(0, "bilibili.search", {"query": "猫"}, after=[]),
        step(1, "bilibili.play", {"index": 2, "search_result_id": "$step_0.output.search_result_id"}, after=[0]),
    ]))
    assert idx.actions[1].params["index"] == 2
    q = validate_decision_proposal(proposal([step(0, "bilibili.play", {"query": "猫"})]))
    assert q.actions[0].params["query"] == "猫"


@pytest.mark.parametrize("params", [
    {"bv": "BV1LqJC6iEZV", "query": "猫"},
    {"index": 2},
    {},
    {"bv": "BVshort"},
])
def test_bad_play_params_rejected(params):
    with pytest.raises(ContractError):
        validate_decision_proposal(proposal([step(0, "bilibili.play", params)]))


def test_unknown_action_and_param_rejected():
    with pytest.raises(ContractError):
        validate_decision_proposal(proposal([step(0, "system.run_applescript", {})]))
    with pytest.raises(ContractError):
        validate_decision_proposal(proposal([step(0, "bilibili.search", {"query": "x", "url": "javascript:1"})]))


def test_forward_reference_rejected():
    with pytest.raises(ContractError):
        validate_decision_proposal(proposal([
            step(0, "bilibili.pause", after=[1]),
            step(1, "bilibili.resume"),
        ]))


def test_self_reference_rejected():
    with pytest.raises(ContractError):
        validate_decision_proposal(proposal([step(0, "bilibili.pause", after=[0])]))


def test_status_reason_consistency():
    with pytest.raises(ContractError):
        validate_decision_proposal(proposal([], status="clarify_needed", reason="none"))
    with pytest.raises(ContractError):
        validate_decision_proposal(proposal([], status="ok", reason="none"))
    with pytest.raises(ContractError):
        validate_decision_proposal(proposal([step(0, "bilibili.pause")], status="clarify_needed", reason="ambiguous"))
    p = validate_decision_proposal(proposal([], status="out_of_scope", reason="unsupported"))
    assert p.actions == []


def test_ids_contiguous_and_step_limit():
    with pytest.raises(ContractError):
        validate_decision_proposal(proposal([step(0, "bilibili.pause"), step(2, "bilibili.resume")]))
    six = [step(i, "bilibili.pause") for i in range(6)]
    with pytest.raises(ContractError):
        validate_decision_proposal(proposal(six))


def test_model_confidence_is_diagnostic_only():
    p = validate_decision_proposal(proposal([step(0, "bilibili.pause")], model_confidence=1.0))
    assert p.model_confidence == 1.0
    with pytest.raises(ContractError):
        validate_decision_proposal(proposal([step(0, "bilibili.pause")], model_confidence=True))


def test_capability_gate_and_reference_shape():
    p = validate_decision_proposal(proposal([
        step(0, "bilibili.search", {"query": "猫"}),
        step(1, "bilibili.play", {"index": 1, "search_result_id": "$step_0.output.search_result_id"}, after=[0]),
    ]))
    validate_plan_structure(p, contracts.R1_ACTIONS)  # R1 play + search allowed
    p2 = validate_decision_proposal(proposal([step(0, "system.mute")]))
    with pytest.raises(ContractError) as exc:
        validate_plan_structure(p2, contracts.R1_ACTIONS)
    assert exc.value.code == "CAPABILITY_UNAVAILABLE"


def test_canonical_json_and_digest_stability():
    a = canonical_json({"b": 2, "a": "猫"})
    b = canonical_json({"a": "猫", "b": 2})
    assert a == b
    assert "猫".encode() in a
    with pytest.raises(ValueError):
        canonical_json({"x": 1.5})


def test_result_message_shape():
    from voice_assistant.contracts import Result, StepResult
    r = Result(
        msg_id=1, command_id="11111111-1111-4111-8111-111111111111",
        state=contracts.CommandState.SUCCEEDED, summary="ok",
        step_results=[StepResult(step_index=0, status=StepState.SUCCEEDED)],
        replayable=True,
    )
    assert r.step_results[0].status is StepState.SUCCEEDED
