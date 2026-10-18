"""Deterministic fake Bilibili adapter for protocol/executor tests."""
from __future__ import annotations

import time
import uuid
from dataclasses import dataclass, field
from typing import Any

from ..contracts import ActionResult, ErrorCode, StepState, ValidatedAction
from .base import (
    ActionAdapter,
    AdapterError,
    CapabilitySet,
    InvocationReceipt,
    PreparedAction,
    ResolvedAction,
)


@dataclass
class FakeBilibiliState:
    app_open: bool = True
    current_video: str | None = "BV1LqJC6iEZV"
    paused: bool = False
    position_ms: int = 30_000
    account_id: str = "test-account"
    danmaku_on: bool = True
    favorites: set[str] = field(default_factory=set)
    likes: set[str] = field(default_factory=set)
    search_results: list[dict[str, Any]] = field(default_factory=lambda: [
        {"video_id": "BV1LqJC6iEZV", "title": "猫猫合集"},
        {"video_id": "BV1xx411c7mD", "title": "机器学习教程"},
        {"video_id": "BV1ab411c7mE", "title": "第三个视频"},
    ])
    history: list[str] = field(default_factory=list)
    invocations: list[tuple[str, dict[str, Any]]] = field(default_factory=list)
    fail_actions: set[str] = field(default_factory=set)
    unknown_actions: set[str] = field(default_factory=set)
    verify_failure_actions: set[str] = field(default_factory=set)
    player_generation: str = field(default_factory=lambda: str(uuid.uuid4()))
    search_generation: str = field(default_factory=lambda: str(uuid.uuid4()))
    search_snapshot_id: str = field(default_factory=lambda: str(uuid.uuid4()))
    search_title: dict[str, str] = field(default_factory=dict)


class FakeBilibiliAdapter(ActionAdapter):
    name = "fake-bilibili"

    def __init__(self, state: FakeBilibiliState | None = None, r2: bool = False):
        self.state = state or FakeBilibiliState()
        self.r2 = r2
        self._prepared: dict[int, dict[str, Any]] = {}

    def probe(self) -> CapabilitySet:
        caps = {"bilibili.open", "bilibili.search", "bilibili.play", "bilibili.pause", "bilibili.resume"}
        if self.r2:
            caps |= {
                "bilibili.next", "bilibili.prev", "bilibili.favorite", "bilibili.like",
                "bilibili.history", "bilibili.danmaku_on", "bilibili.danmaku_off",
                "bilibili.send_danmaku",
            }
        return CapabilitySet(caps)

    def _check_video(self, binding: dict[str, Any]) -> None:
        expected = binding.get("video_id")
        if expected and self.state.current_video and expected != self.state.current_video:
            raise AdapterError(ErrorCode.TARGET_STALE, "current video changed")
        if binding.get("generation") and binding["generation"] != self.state.player_generation:
            raise AdapterError(ErrorCode.TARGET_STALE, "player generation changed")

    def prepare(self, resolved: ResolvedAction, context: Any) -> PreparedAction:
        action = resolved.action
        params = resolved.params
        if action in self.state.fail_actions:
            raise AdapterError(ErrorCode.ACTION_FAILED, f"fake failure for {action}")
        if action == "bilibili.open":
            if not self.state.app_open:
                return PreparedAction(resolved, {"launch": True}, time.time())
            return PreparedAction(resolved, {"noop": True}, time.time())
        if action == "bilibili.search":
            query = params.get("query", "")
            if not query:
                raise AdapterError(ErrorCode.INVALID_PLAN, "empty query")
            return PreparedAction(resolved, {"query": query}, time.time())
        if action == "bilibili.play":
            if not self.state.app_open:
                raise AdapterError(ErrorCode.TARGET_STALE, "Bilibili is not running")
            binding = resolved.binding
            if "video_id" in binding and "index" not in params:
                target = binding["video_id"]
                title = self.state.search_title.get(target, target)
                return PreparedAction(resolved, {"video_id": target, "title": title}, time.time())
            index = int(params.get("index", 1))
            snapshot_id = params.get("search_result_id") or binding.get("search_result_id")
            if snapshot_id and str(snapshot_id) != self.state.search_snapshot_id:
                raise AdapterError(ErrorCode.TARGET_STALE, "search snapshot changed")
            if index < 1 or index > len(self.state.search_results):
                raise AdapterError(ErrorCode.TARGET_STALE, "search index out of range")
            target = self.state.search_results[index - 1]
            return PreparedAction(resolved, {"video_id": target["video_id"], "title": target["title"]}, time.time())
        if action in {"bilibili.pause", "bilibili.resume"}:
            self._check_video(resolved.binding)
            if not self.state.current_video:
                raise AdapterError(ErrorCode.TARGET_STALE, "no current video")
            return PreparedAction(resolved, {"target_paused": action == "bilibili.pause"}, time.time())
        if action in {"bilibili.danmaku_on", "bilibili.danmaku_off"}:
            self._check_video(resolved.binding)
            return PreparedAction(resolved, {"on": action.endswith("_on")}, time.time())
        if action in {"bilibili.next", "bilibili.prev"}:
            return PreparedAction(resolved, {"direction": action}, time.time())
        if action in {"bilibili.favorite", "bilibili.like"}:
            self._check_video(resolved.binding)
            vid = resolved.binding.get("video_id") or self.state.current_video
            if not vid:
                raise AdapterError(ErrorCode.TARGET_STALE, "no target video")
            return PreparedAction(resolved, {"video_id": vid}, time.time())
        if action == "bilibili.send_danmaku":
            self._check_video(resolved.binding)
            text = params.get("text", "")
            if not text:
                raise AdapterError(ErrorCode.INVALID_PLAN, "empty danmaku")
            return PreparedAction(resolved, {"video_id": self.state.current_video, "text": text}, time.time())
        if action == "bilibili.history":
            return PreparedAction(resolved, {}, time.time())
        raise AdapterError(ErrorCode.CAPABILITY_UNAVAILABLE, "fake adapter does not implement action")

    def invoke(self, prepared: PreparedAction, deadline: float) -> InvocationReceipt:
        started = time.time()
        action = prepared.resolved.action
        if action in self.state.unknown_actions:
            return InvocationReceipt(prepared, started, time.time(), True, {"unknown": True})
        if action in self.state.verify_failure_actions:
            return InvocationReceipt(prepared, started, time.time(), True, {"verification_failure": True})
        p = prepared.prepared or {}
        self.state.invocations.append((action, dict(p)))
        if action == "bilibili.open":
            self.state.app_open = True
        elif action == "bilibili.search":
            self.state.search_snapshot_id = str(uuid.uuid4())
            self.state.search_generation = str(uuid.uuid4())
            for i, item in enumerate(self.state.search_results, start=1):
                item["index"] = i
        elif action == "bilibili.play":
            self.state.current_video = p["video_id"]
            self.state.paused = False
            self.state.position_ms = 0
            self.state.player_generation = str(uuid.uuid4())
            self.state.history.append(p["video_id"])
        elif action == "bilibili.pause":
            self.state.paused = True
        elif action == "bilibili.resume":
            self.state.paused = False
        elif action == "bilibili.danmaku_on":
            self.state.danmaku_on = True
        elif action == "bilibili.danmaku_off":
            self.state.danmaku_on = False
        elif action == "bilibili.favorite":
            self.state.favorites.add(p["video_id"])
        elif action == "bilibili.like":
            self.state.likes.add(p["video_id"])
        elif action == "bilibili.send_danmaku":
            pass
        return InvocationReceipt(prepared, started, time.time(), True, {})

    def verify(self, receipt: InvocationReceipt, deadline: float) -> ActionResult:
        action = receipt.prepared.resolved.action
        p = receipt.prepared.prepared or {}
        latency = max(0, int((receipt.finished_at - receipt.started_at) * 1000))
        if action in self.state.verify_failure_actions:
            return ActionResult(step_index=receipt.prepared.resolved.step_index, status=StepState.FAILED,
                                code=ErrorCode.ACTION_FAILED, evidence={"verified": False}, latency_ms=latency)
        output: dict[str, Any] = {}
        status = StepState.SUCCEEDED
        if action == "bilibili.open":
            output = {"app_id": "bilibili"}
        elif action == "bilibili.search":
            output = {
                "search_result_id": self.state.search_snapshot_id,
                "results": self.state.search_results,
                "generation": self.state.search_generation,
            }
        elif action == "bilibili.play":
            output = {"video_id": self.state.current_video, "player_generation": self.state.player_generation}
        elif action == "bilibili.pause":
            if not self.state.paused:
                return ActionResult(step_index=receipt.prepared.resolved.step_index, status=StepState.FAILED,
                                    code=ErrorCode.ACTION_FAILED, latency_ms=latency)
        elif action == "bilibili.resume":
            if self.state.paused:
                return ActionResult(step_index=receipt.prepared.resolved.step_index, status=StepState.FAILED,
                                    code=ErrorCode.ACTION_FAILED, latency_ms=latency)
        elif action == "bilibili.danmaku_on":
            if not self.state.danmaku_on:
                return ActionResult(step_index=receipt.prepared.resolved.step_index, status=StepState.FAILED,
                                    code=ErrorCode.ACTION_FAILED, latency_ms=latency)
        elif action == "bilibili.danmaku_off":
            if self.state.danmaku_on:
                return ActionResult(step_index=receipt.prepared.resolved.step_index, status=StepState.FAILED,
                                    code=ErrorCode.ACTION_FAILED, latency_ms=latency)
        elif action == "bilibili.favorite":
            if p["video_id"] not in self.state.favorites:
                return ActionResult(step_index=receipt.prepared.resolved.step_index, status=StepState.FAILED,
                                    code=ErrorCode.ACTION_FAILED, latency_ms=latency)
            if p["video_id"] in self.state.favorites:
                status = StepState.SUCCEEDED
        elif action == "bilibili.like":
            if p["video_id"] not in self.state.likes:
                return ActionResult(step_index=receipt.prepared.resolved.step_index, status=StepState.FAILED,
                                    code=ErrorCode.ACTION_FAILED, latency_ms=latency)
        evidence = {"verified": True, "action": action}
        return ActionResult(step_index=receipt.prepared.resolved.step_index, status=status,
                            output=output, evidence=evidence, latency_ms=latency)
