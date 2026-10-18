from __future__ import annotations

import asyncio
import json
import socket
import ssl
import sys
import threading
import time
import uuid
from pathlib import Path

import pytest
import uvicorn
import websockets

sys.path.insert(0, str(Path(__file__).resolve().parents[2] / "src"))

from voice_assistant.adapters.base import AdapterRegistry  # noqa: E402
from voice_assistant.adapters.fake import FakeBilibiliAdapter  # noqa: E402
from voice_assistant.api import create_app  # noqa: E402
from voice_assistant.config import load_config  # noqa: E402
from voice_assistant.decision import FakeDecision  # noqa: E402
from voice_assistant.stt import FakeSTT  # noqa: E402
from voice_assistant.storage import Database  # noqa: E402

R1 = ("bilibili.open", "bilibili.search", "bilibili.play", "bilibili.pause", "bilibili.resume")


@pytest.fixture(scope="module")
def tls_server(tmp_path_factory):
    import os
    os.environ["VOICE_ASSISTANT_DISABLE_DISCOVERY"] = "1"
    root = tmp_path_factory.mktemp("tls")
    cfg = load_config(root)
    db = Database(cfg.db_path)
    device_id, token = db.register_device("tls device")
    fake = FakeBilibiliAdapter()
    registry = AdapterRegistry()
    registry.register(fake, R1)
    app = create_app(cfg, stt=FakeSTT("打开B站"), decision=FakeDecision(), registry=registry)
    # ephemeral port
    sock = socket.socket()
    sock.bind(("127.0.0.1", 0))
    port = sock.getsockname()[1]
    sock.close()
    config = uvicorn.Config(
        app, host="127.0.0.1", port=port,
        ssl_certfile=str(cfg.server.server_cert_path),
        ssl_keyfile=str(cfg.server.server_key_path),
        log_level="warning", access_log=False,
    )
    server = uvicorn.Server(config)
    errors = []
    def run():
        try:
            server.run()
        except BaseException as exc:
            errors.append(repr(exc))
            raise
    thread = threading.Thread(target=run, daemon=True)
    thread.start()
    deadline = time.time() + 10
    while not server.started and time.time() < deadline:
        time.sleep(0.05)
    if not server.started:
        Path("/tmp/tlsdiag.txt").write_text(repr({"errors": errors, "alive": thread.is_alive(),
                                                  "should_exit": server.should_exit, "port": port}))
    assert server.started
    yield cfg, db, device_id, token, port
    server.should_exit = True
    thread.join(timeout=10)
    db.close()


def _hello(cfg, device_id):
    return {
        "v": 1, "type": "hello", "msg_id": 1, "device_id": device_id,
        "server_id": cfg.server.server_id, "firmware_version": "desk-mate-bili/1.0",
        "boot_id": str(uuid.uuid4()),
    }


def test_tls_valid_ca_and_name(tls_server):
    cfg, db, device_id, token, port = tls_server
    ctx = ssl.create_default_context(cafile=str(cfg.server.ca_cert_path))
    async def go():
        async with websockets.connect(
            f"wss://127.0.0.1:{port}/v1/remote", ssl=ctx,
            server_hostname=cfg.server.tls_name,
            additional_headers={"Authorization": f"Bearer {token}", "X-Device-ID": device_id},
        ) as ws:
            await ws.send(json.dumps(_hello(cfg, device_id)))
            ack = json.loads(await ws.recv())
            assert ack["type"] == "hello_ack"
            assert ack["ready"] is True
    asyncio.run(go())


def test_wrong_hostname_rejected(tls_server):
    cfg, db, device_id, token, port = tls_server
    ctx = ssl.create_default_context(cafile=str(cfg.server.ca_cert_path))
    async def go():
        with pytest.raises(ssl.SSLError):
            async with websockets.connect(
                f"wss://127.0.0.1:{port}/v1/remote", ssl=ctx,
                server_hostname="wrong-name.local",
                additional_headers={"Authorization": f"Bearer {token}", "X-Device-ID": device_id},
            ):
                pass
    asyncio.run(go())


def test_wrong_ca_rejected(tls_server, tmp_path_factory):
    cfg, db, device_id, token, port = tls_server
    other = load_config(tmp_path_factory.mktemp("other"))
    ctx = ssl.create_default_context(cafile=str(other.server.ca_cert_path))
    async def go():
        with pytest.raises(ssl.SSLError):
            async with websockets.connect(
                f"wss://127.0.0.1:{port}/v1/remote", ssl=ctx,
                server_hostname=cfg.server.tls_name,
                additional_headers={"Authorization": f"Bearer {token}", "X-Device-ID": device_id},
            ):
                pass
    asyncio.run(go())


def test_bad_token_rejected(tls_server):
    cfg, db, device_id, token, port = tls_server
    ctx = ssl.create_default_context(cafile=str(cfg.server.ca_cert_path))
    async def go():
        with pytest.raises(Exception):
            async with websockets.connect(
                f"wss://127.0.0.1:{port}/v1/remote", ssl=ctx,
                server_hostname=cfg.server.tls_name,
                additional_headers={"Authorization": "Bearer bad", "X-Device-ID": device_id},
            ):
                pass
    asyncio.run(go())
