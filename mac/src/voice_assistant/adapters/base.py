"""Action adapter interfaces: the only place external effects may occur."""
from __future__ import annotations

import time
from dataclasses import dataclass, field
from typing import Any, Protocol

from ..contracts import ActionResult, ErrorCode, ValidatedAction


class AdapterError(RuntimeError):
    def __init__(self, code: ErrorCode, summary: str, effect_possible: bool = False):
        super().__init__(summary)
        self.code = code
        self.summary = summary
        self.effect_possible = effect_possible


@dataclass
class CapabilitySet:
    available: set[str] = field(default_factory=set)
    reasons: dict[str, str] = field(default_factory=dict)

    def has(self, action: str) -> bool:
        return action in self.available

    def describe(self) -> dict[str, Any]:
        return {"available": sorted(self.available), "reasons": dict(self.reasons)}


@dataclass
class ResolvedAction:
    step_index: int
    action: str
    params: dict[str, Any]
    binding: dict[str, Any]


@dataclass
class PreparedAction:
    resolved: ResolvedAction
    prepared: Any = None
    prepared_at: float = 0.0


@dataclass
class InvocationReceipt:
    prepared: PreparedAction
    started_at: float
    finished_at: float
    effect_possible: bool = True
    raw: dict[str, Any] = field(default_factory=dict)


class ActionAdapter(Protocol):
    name: str

    def probe(self) -> CapabilitySet:
        ...

    def prepare(self, resolved: ResolvedAction, context: Any) -> PreparedAction:
        ...

    def invoke(self, prepared: PreparedAction, deadline: float) -> InvocationReceipt:
        ...

    def verify(self, receipt: InvocationReceipt, deadline: float) -> ActionResult:
        ...


class AdapterRegistry:
    """Routes actions to adapters; exactly one adapter may own an action."""

    def __init__(self) -> None:
        self._routes: dict[str, ActionAdapter] = {}
        self._adapters: list[ActionAdapter] = []

    def register(self, adapter: ActionAdapter, actions: tuple[str, ...]) -> None:
        for action in actions:
            if action in self._routes:
                raise ValueError(f"duplicate adapter for {action}")
            self._routes[action] = adapter
        self._adapters.append(adapter)

    def adapter_for(self, action: str) -> ActionAdapter:
        try:
            return self._routes[action]
        except KeyError as exc:
            raise AdapterError(ErrorCode.CAPABILITY_UNAVAILABLE, f"no adapter for {action}") from exc

    def capabilities(self) -> CapabilitySet:
        result = CapabilitySet()
        for adapter in self._adapters:
            caps = adapter.probe()
            result.available |= caps.available
            result.reasons.update(caps.reasons)
        return result
