"""Authenticated WSS control server for the ESP32 voice feature."""
from __future__ import annotations

import asyncio
import json
import logging
import os
import time
import uuid
from contextlib import asynccontextmanager
from typing import Any
from uuid import UUID

from fastapi import FastAPI, WebSocket, WebSocketDisconnect
from fastapi.responses import JSONResponse

from . import contracts
from .adapters.base import AdapterRegistry
from .config import AppConfig, load_config
from .contracts import (
    CancelAck,
    CancelOutcome,
    BeginAck,
    CommitAck,
    CommandState,
    ConfirmRequired,
    ErrorCode,
    ErrorMessage,
    Feedback,
    FeedbackAck,
    Hello,
    HelloAck,
    Limits,
    Ping,
    Pong,
    Progress,
    ReplayReady,
    Result,
    SealAck,
    StatusSnapshot,
    StepResult,
    StepState,
    TERMINAL_STATES,
    decode_json,
    parse_client_message,
)
from .decision import DecisionAdapter, OllamaDecision
from .discovery import DiscoveryPublisher
from .pipeline import Pipeline, PipelineContext
from .sessions import CommandService, SessionError, StageStore
from .stt import STTAdapter, build_stt
from .storage import Database, StorageError, utc_now

log = logging.getLogger("voice_assistant.api")
FIRMWARE_VERSION_MAX = 64


def _json(model: Any) -> str:
    return model.model_dump_json()


class ConnectionManager:
    def __init__(self) -> None:
        self._connections: dict[str, WebSocket] = {}
        self._lock = asyncio.Lock()

    async def attach(self, device_id: str, ws: WebSocket) -> None:
        async with self._lock:
            old = self._connections.get(device_id)
            self._connections[device_id] = ws
        if old is not None and old is not ws:
            try:
                await old.close(code=1008, reason="replaced by a newer device connection")
            except Exception:
                pass

    async def detach(self, device_id: str, ws: WebSocket) -> None:
        async with self._lock:
            if self._connections.get(device_id) is ws:
                self._connections.pop(device_id, None)

    async def send(self, device_id: str, message: Any) -> bool:
        async with self._lock:
            ws = self._connections.get(device_id)
        if ws is None:
            return False
        try:
            await ws.send_text(_json(message))
            return True
        except Exception:
            return False

    async def close(self, device_id: str) -> None:
        async with self._lock:
            ws = self._connections.pop(device_id, None)
        if ws is not None:
            try:
                await ws.close(code=1008)
            except Exception:
                pass


class AppState:
    def __init__(
        self,
        cfg: AppConfig,
        db: Database,
        stages: StageStore,
        service: CommandService,
        stt: STTAdapter,
        decision: DecisionAdapter,
        registry: AdapterRegistry,
    ):
        self.cfg = cfg
        self.db = db
        self.stages = stages
        self.service = service
        self.stt = stt
        self.decision = decision
        self.registry = registry
        self.enabled_actions: tuple[str, ...] = contracts.R1_ACTIONS
        self.manager = ConnectionManager()
        self.tasks: set[asyncio.Task] = set()
        from .personalization import Personalizer
        self.personalizer = Personalizer(cfg)
        self.pipeline = Pipeline(PipelineContext(
            cfg=cfg, db=db, service=service, stt=stt, decision=decision,
            executor=None, enabled_actions=self.enabled_actions,
            personalizer=self.personalizer,
        ))
        from .executor import Executor
        self.pipeline.ctx.executor = Executor(db, registry)
        try:
            billi = registry.adapter_for("bilibili.pause")
            if hasattr(billi, "refresh_player_snapshot"):
                self.pipeline.ctx.refresh_player = billi.refresh_player_snapshot
            self.bilibili_adapter = billi
        except Exception:
            self.bilibili_adapter = None
        self.discovery: DiscoveryPublisher | None = None
        self.started_at = utc_now()

    async def notify_for(self, device_id: str):
        async def notify(message: Any) -> None:
            await self.manager.send(device_id, message)
        return notify

    def capabilities(self) -> tuple[str, ...]:
        caps = self.registry.capabilities()
        return tuple(a for a in self.enabled_actions if caps.has(a))

    def ready(self) -> tuple[bool, ErrorCode | None]:
        if not self.cfg.ready:
            return False, ErrorCode.READY_FALSE
        caps = self.capabilities()
        if not caps:
            return False, ErrorCode.CAPABILITY_UNAVAILABLE
        health = self.stt.health()
        if not health.get("available", False):
            return False, ErrorCode.MODEL_UNAVAILABLE
        dec = self.decision.health()
        if not dec.get("available", False):
            return False, ErrorCode.MODEL_UNAVAILABLE
        return True, None


def _error(msg_id: int, code: ErrorCode, summary: str, command_id: UUID | None = None,
           in_reply_to: int | None = None, fatal: bool = False) -> ErrorMessage:
    return ErrorMessage(
        msg_id=max(1, msg_id), in_reply_to=in_reply_to or (msg_id if msg_id else None),
        command_id=command_id, code=code, summary=summary[:128], fatal=fatal,
    )


def create_app(
    cfg: AppConfig | None = None,
    *,
    stt: STTAdapter | None = None,
    decision: DecisionAdapter | None = None,
    registry: AdapterRegistry | None = None,
) -> FastAPI:
    cfg = cfg or load_config()
    cfg.ensure_layout()
    db = Database(cfg.db_path)
    reconcile = db.reconcile_startup()
    stages = StageStore(cfg, db)
    service = CommandService(cfg, db, stages)
    stt = stt or build_stt(cfg.stt_backend)
    decision = decision or OllamaDecision(cfg.ollama_model, cfg.ollama_url)
    if registry is None:
        from .adapters.bilibili import build_bilibili_adapter
        from .adapters.macos import MacSystemAdapter
        registry = AdapterRegistry()
        billi = build_bilibili_adapter(cfg, db)
        registry.register(billi, tuple(a for a in contracts.ALL_ACTIONS if a.startswith("bilibili.")))
        # R2 system actions are registered but remain disabled until enabled.
        registry.register(MacSystemAdapter(cfg, db),
                          tuple(a for a in contracts.R2_ACTIONS if a.startswith("system.")))
    state = AppState(cfg, db, stages, service, stt, decision, registry)

    @asynccontextmanager
    async def lifespan(app: FastAPI):
        # Prewarm the local STT worker; model loading is not allowed to block
        # the socket event loop (it runs in a worker thread/process).
        try:
            await asyncio.to_thread(stt.health)
        except Exception as exc:
            log.warning("STT prewarm failed: %s", exc)
        if getattr(state, "bilibili_adapter", None) is not None:
            try:
                await asyncio.to_thread(state.bilibili_adapter.start_daemon)
            except Exception as exc:
                log.warning("AX daemon prewarm failed: %s", exc)
        if getattr(state, "personalizer", None) is not None:
            try:
                await asyncio.to_thread(state.personalizer._load)
            except Exception as exc:
                log.warning("personalizer prewarm failed: %s", exc)

        async def training_reminder_loop() -> None:
            from .training import maybe_remind
            while True:
                try:
                    result = await asyncio.to_thread(maybe_remind, db, True)
                    if result.get("samples", 0):
                        log.info("training status: %s", result)
                except Exception as exc:
                    log.warning("training reminder failed: %s", exc)
                await asyncio.sleep(6 * 3600)

        state.training_task = asyncio.create_task(training_reminder_loop())
        if os.environ.get("VOICE_ASSISTANT_DISABLE_DISCOVERY") != "1":
            state.discovery = DiscoveryPublisher(cfg)
            try:
                state.discovery.start()
            except Exception as exc:
                log.warning("mDNS discovery not started: %s", exc)
        yield
        if getattr(state, "training_task", None):
            state.training_task.cancel()
        for task in list(state.tasks):
            task.cancel()
        if state.discovery:
            state.discovery.stop()
        await state.manager.close_all() if hasattr(state.manager, "close_all") else None
        db.close()

    app = FastAPI(title="Desk-Mate Voice Assistant", version="1.0", lifespan=lifespan)
    app.state.voice = state

    @app.get("/healthz")
    async def healthz():
        ready, code = state.ready()
        return {
            "ok": True,
            "ready": ready,
            "code": code.value if code else None,
            "server_id": cfg.server.server_id if cfg.server else None,
            "tls_name": cfg.server.tls_name if cfg.server else None,
            "stt": stt.health(),
            "decision": decision.health(),
            "capabilities": list(state.capabilities()),
            "reconciled": reconcile,
            "active_command": (db.active_command() or {}).get("id"),
            "started_at": state.started_at,
        }

    @app.get("/v1/status/{command_id}")
    async def http_status(command_id: str):
        row = db.get_command(command_id)
        if not row:
            return JSONResponse({"known": False, "code": "COMMAND_NOT_FOUND"}, status_code=404)
        msg = state.pipeline.result_message(command_id)
        return {
            "known": True,
            "state": row["state"],
            "terminal": CommandState(row["state"]) in TERMINAL_STATES,
            "result": msg.model_dump(mode="json") if msg else None,
        }

    @app.websocket("/v1/remote")
    async def remote(ws: WebSocket):
        headers = {k.lower(): v for k, v in ws.headers.items()}
        origin = headers.get("origin")
        if origin and origin not in {"null"}:
            await ws.close(code=1008, reason="browser origins are not allowed")
            return
        auth = headers.get("authorization", "")
        device_id = headers.get("x-device-id", "")
        token = auth[7:].strip() if auth.lower().startswith("bearer ") else ""
        if not device_id or not token or not db.authenticate_device(device_id, token):
            await ws.close(code=1008, reason="authentication failed")
            return
        await ws.accept()
        await state.manager.attach(device_id, ws)
        hello_done = False
        active_command: str | None = None
        last_ack = 0.0
        pending_frames = 0
        try:
            while True:
                message = await ws.receive()
                if message.get("type") == "websocket.disconnect":
                    break
                if "bytes" in message and message["bytes"] is not None:
                    data = message["bytes"]
                    if not hello_done or not active_command:
                        await ws.send_text(_json(_error(0, ErrorCode.PROTOCOL_ERROR, "binary frame outside begin")))
                        continue
                    row = db.get_command(active_command)
                    if not row or row["state"] != CommandState.STAGING.value:
                        await ws.send_text(_json(_error(0, ErrorCode.PROTOCOL_ERROR, "binary frame outside STAGING")))
                        continue
                    try:
                        receipt = stages.append(active_command, data)
                    except (SessionError, Exception) as exc:
                        code = getattr(exc, "code", ErrorCode.AUDIO_INVALID)
                        if not isinstance(code, ErrorCode):
                            try:
                                code = ErrorCode(str(code))
                            except Exception:
                                code = ErrorCode.AUDIO_INVALID
                        try:
                            stages.cancel(active_command, "audio_error")
                        except Exception:
                            pass
                        await ws.send_text(_json(_error(
                            0, code, str(exc)[:128], UUID(active_command), fatal=True,
                        )))
                        active_command = None
                        continue
                    pending_frames += 1
                    now = time.time()
                    if receipt.should_ack or now - last_ack >= 0.2:
                        last_ack = now
                        pending_frames = 0
                        await ws.send_text(_json(contracts.AppendAck(
                            command_id=UUID(active_command), next_seq=receipt.next_seq,
                            pcm_bytes=receipt.pcm_bytes,
                        )))
                    continue
                text = message.get("text")
                if text is None:
                    continue
                try:
                    parsed = parse_client_message(text)
                except contracts.ContractError as exc:
                    await ws.send_text(_json(_error(0, ErrorCode.PROTOCOL_ERROR, str(exc)[:128], fatal=False)))
                    continue
                if isinstance(parsed, Ping):
                    ready, code = state.ready()
                    await ws.send_text(_json(Pong(msg_id=parsed.msg_id, in_reply_to=parsed.msg_id,
                                                  ready=ready, code=code)))
                    continue
                if isinstance(parsed, Hello):
                    if hello_done:
                        await ws.send_text(_json(_error(parsed.msg_id, ErrorCode.PROTOCOL_ERROR, "duplicate hello")))
                        continue
                    if cfg.server is None or str(parsed.server_id) != cfg.server.server_id:
                        await ws.send_text(_json(_error(parsed.msg_id, ErrorCode.PAIRING_REQUIRED,
                                                        "server identity mismatch", fatal=True)))
                        await ws.close(code=1008)
                        return
                    if str(parsed.device_id) != device_id:
                        await ws.send_text(_json(_error(parsed.msg_id, ErrorCode.AUTH_FAILED, "device id mismatch", fatal=True)))
                        await ws.close(code=1008)
                        return
                    ready, code = state.ready()
                    db.record_session(str(parsed.boot_id), device_id, parsed.firmware_version)
                    await ws.send_text(_json(HelloAck(
                        msg_id=parsed.msg_id, in_reply_to=parsed.msg_id,
                        server_id=UUID(cfg.server.server_id), device_id=UUID(device_id),
                        ready=ready, code=code, capabilities=list(state.capabilities()),
                        limits=Limits(), capability_version=contracts.CAPABILITY_VERSION,
                    )))
                    hello_done = True
                    continue
                if not hello_done:
                    await ws.send_text(_json(_error(parsed.msg_id, ErrorCode.PROTOCOL_ERROR, "hello required", fatal=True)))
                    await ws.close(code=1008)
                    return
                if isinstance(parsed, Feedback):
                    try:
                        row = db.get_command(str(parsed.command_id))
                        if not row:
                            raise ValueError("command not found")
                        if row.get("device_id") and str(row["device_id"]) != device_id:
                            raise ValueError("feedback device mismatch")
                        saved = db.record_feedback(
                            str(parsed.command_id), device_id,
                            rating=parsed.rating, dismissed_by=parsed.dismissed_by,
                            touch_count=parsed.touch_count,
                            countdown_at_dismiss=parsed.countdown_at_dismiss,
                            page=parsed.page, source="device",
                        )
                    except Exception as exc:
                        await ws.send_text(_json(_error(
                            parsed.msg_id, ErrorCode.PROTOCOL_ERROR, str(exc)[:128],
                            parsed.command_id,
                        )))
                        continue
                    await ws.send_text(_json(FeedbackAck(
                        msg_id=0, in_reply_to=parsed.msg_id,
                        command_id=parsed.command_id, accepted=True,
                        source=str(saved.get("source", "device")),
                    )))
                    try:
                        from . import feedback_analysis
                        if feedback_analysis.due(db) and not getattr(state, "analysis_running", False):
                            state.analysis_running = True
                            async def _analyze_feedback() -> None:
                                try:
                                    from .auto_training import train_if_ready
                                    await asyncio.to_thread(feedback_analysis.analyze, cfg, db)
                                    await asyncio.to_thread(train_if_ready, cfg, db)
                                except Exception as exc:
                                    log.warning("feedback auto-analysis failed: %s", exc)
                                finally:
                                    state.analysis_running = False
                            task = asyncio.create_task(_analyze_feedback())
                            state.tasks.add(task)
                            task.add_done_callback(state.tasks.discard)
                    except Exception as exc:
                        log.warning("feedback auto-analysis scheduling failed: %s", exc)
                    continue
                if isinstance(parsed, contracts.Begin):
                    if active_command or db.active_command():
                        await ws.send_text(_json(_error(parsed.msg_id, ErrorCode.BUSY, "another command is active")))
                        continue
                    try:
                        stages.begin(str(parsed.command_id), device_id, None)
                    except Exception as exc:
                        code = getattr(exc, "code", ErrorCode.BUSY)
                        try:
                            code = ErrorCode(str(code))
                        except Exception:
                            code = ErrorCode.BUSY
                        await ws.send_text(_json(_error(parsed.msg_id, code, str(exc)[:128])))
                        continue
                    active_command = str(parsed.command_id)
                    last_ack = time.time()
                    await ws.send_text(_json(BeginAck(
                        msg_id=parsed.msg_id, in_reply_to=parsed.msg_id,
                        command_id=parsed.command_id,
                    )))
                    continue
                if isinstance(parsed, contracts.ReplayPrepare):
                    if active_command or db.active_command():
                        await ws.send_text(_json(_error(parsed.msg_id, ErrorCode.BUSY, "another command is active")))
                        continue
                    try:
                        ready = service.prepare_replay(
                            str(parsed.command_id), device_id, str(parsed.replay_of), boot_id=None,
                        )
                    except Exception as exc:
                        code = getattr(exc, "code", ErrorCode.COMMAND_NOT_FOUND)
                        try:
                            code = ErrorCode(str(code))
                        except Exception:
                            code = ErrorCode.COMMAND_NOT_FOUND
                        await ws.send_text(_json(_error(parsed.msg_id, code, str(exc)[:128])))
                        continue
                    active_command = str(parsed.command_id)
                    await ws.send_text(_json(ReplayReady(
                        msg_id=parsed.msg_id, in_reply_to=parsed.msg_id,
                        command_id=parsed.command_id, replay_of=parsed.replay_of,
                        sha256=ready["sha256"], expires_in_ms=ready["expires_in_ms"],
                        summary=ready["summary"],
                    )))
                    continue
                if isinstance(parsed, contracts.Seal):
                    if not active_command or str(parsed.command_id) != active_command:
                        await ws.send_text(_json(_error(parsed.msg_id, ErrorCode.PROTOCOL_ERROR, "no matching active command")))
                        continue
                    try:
                        row = stages.seal(active_command, parsed.frame_count, parsed.pcm_bytes, parsed.sha256)
                    except Exception as exc:
                        code = getattr(exc, "code", ErrorCode.AUDIO_INVALID)
                        try:
                            code = ErrorCode(str(code))
                        except Exception:
                            code = ErrorCode.AUDIO_INVALID
                        await ws.send_text(_json(_error(parsed.msg_id, code, str(exc)[:128],
                                                        UUID(active_command))))
                        active_command = None
                        continue
                    await ws.send_text(_json(SealAck(
                        msg_id=parsed.msg_id, in_reply_to=parsed.msg_id,
                        command_id=parsed.command_id, frame_count=row["frame_count"],
                        pcm_bytes=row["pcm_bytes"], sha256=row["pcm_sha256"],
                    )))
                    continue
                if isinstance(parsed, contracts.Commit):
                    if not active_command or str(parsed.command_id) != active_command:
                        await ws.send_text(_json(_error(parsed.msg_id, ErrorCode.PROTOCOL_ERROR, "no matching active command")))
                        continue
                    try:
                        row = stages.commit(active_command, parsed.sha256)
                    except Exception as exc:
                        code = getattr(exc, "code", ErrorCode.STORAGE_ERROR)
                        try:
                            code = ErrorCode(str(code))
                        except Exception:
                            code = ErrorCode.STORAGE_ERROR
                        await ws.send_text(_json(_error(parsed.msg_id, code, str(exc)[:128],
                                                        UUID(active_command))))
                        continue
                    await ws.send_text(_json(CommitAck(
                        msg_id=parsed.msg_id, in_reply_to=parsed.msg_id,
                        command_id=parsed.command_id, state=CommandState(row["state"]), code=None,
                    )))
                    active_command = None
                    notify = await state.notify_for(device_id)
                    task = asyncio.create_task(state.pipeline.process(active_command or str(parsed.command_id), device_id, notify))
                    state.tasks.add(task)
                    task.add_done_callback(state.tasks.discard)
                    continue
                if isinstance(parsed, contracts.Cancel):
                    try:
                        outcome, st = stages.cancel(str(parsed.command_id), parsed.reason.value)
                    except Exception as exc:
                        code = getattr(exc, "code", ErrorCode.COMMAND_NOT_FOUND)
                        try:
                            code = ErrorCode(str(code))
                        except Exception:
                            code = ErrorCode.COMMAND_NOT_FOUND
                        await ws.send_text(_json(_error(parsed.msg_id, code, str(exc)[:128])))
                        continue
                    if active_command == str(parsed.command_id):
                        active_command = None
                    await ws.send_text(_json(CancelAck(
                        msg_id=parsed.msg_id, in_reply_to=parsed.msg_id,
                        command_id=parsed.command_id, state=st, outcome=CancelOutcome(outcome),
                    )))
                    continue
                if isinstance(parsed, contracts.StatusQuery):
                    row = db.get_command(str(parsed.command_id))
                    if not row:
                        await ws.send_text(_json(StatusSnapshot(
                            msg_id=parsed.msg_id, in_reply_to=parsed.msg_id,
                            command_id=parsed.command_id, known=False, state=None,
                            terminal=False, code=ErrorCode.COMMAND_NOT_FOUND, result=None,
                        )))
                        continue
                    st = CommandState(row["state"])
                    result = state.pipeline.result_message(str(parsed.command_id)) if st in TERMINAL_STATES else None
                    await ws.send_text(_json(StatusSnapshot(
                        msg_id=parsed.msg_id, in_reply_to=parsed.msg_id,
                        command_id=parsed.command_id, known=True, state=st,
                        terminal=st in TERMINAL_STATES,
                        code=ErrorCode(row["error_code"]) if row["error_code"] else None,
                        result=result,
                    )))
                    continue
                if isinstance(parsed, contracts.Confirm):
                    # R2 confirmation execution is implemented by the R2 adapter
                    # work; R1 fails closed rather than executing an unconfirmed plan.
                    await ws.send_text(_json(_error(parsed.msg_id, ErrorCode.CAPABILITY_UNAVAILABLE,
                                                    "confirmation is not enabled in R1")))
                    continue
                await ws.send_text(_json(_error(parsed.msg_id, ErrorCode.PROTOCOL_ERROR, "unsupported message")))
        except WebSocketDisconnect:
            pass
        except Exception as exc:
            log.exception("websocket error: %s", exc)
        finally:
            if active_command:
                row = db.get_command(active_command)
                if row and row["state"] in (CommandState.STAGING.value, CommandState.SEALED.value):
                    try:
                        stages.cancel(active_command, "network_error")
                    except Exception:
                        pass
            await state.manager.detach(device_id, ws)

    return app


def run_server(cfg: AppConfig | None = None) -> None:
    import uvicorn

    cfg = cfg or load_config()
    app = create_app(cfg)
    if cfg.server is None:
        raise SystemExit("server identity missing")
    uvicorn.run(
        app,
        host="0.0.0.0",
        port=cfg.port,
        ssl_certfile=str(cfg.server.server_cert_path),
        ssl_keyfile=str(cfg.server.server_key_path),
        log_level="info",
        access_log=False,
        ws_ping_interval=30,
        ws_ping_timeout=10,
    )
