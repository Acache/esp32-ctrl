"""mDNS publication: the Mac advertises, the ESP32 discovers. Identity is TLS, not mDNS."""
from __future__ import annotations

import socket
import time

from zeroconf import ServiceInfo, Zeroconf

from .config import AppConfig, lan_ip

SERVICE_TYPE = "_voice-assistant._tcp.local."


class DiscoveryPublisher:
    def __init__(self, cfg: AppConfig):
        if cfg.server is None:
            raise RuntimeError("server identity is required for discovery")
        self.cfg = cfg
        self.zc: Zeroconf | None = None
        self.info: ServiceInfo | None = None

    def start(self) -> None:
        ip = lan_ip()
        addresses = []
        if ip:
            try:
                addresses.append(socket.inet_aton(ip))
            except OSError:
                pass
        if not addresses:
            addresses = [socket.inet_aton("127.0.0.1")]
        self.info = ServiceInfo(
            SERVICE_TYPE,
            f"{self.cfg.server.tls_name}.{SERVICE_TYPE}",
            addresses=addresses,
            port=self.cfg.port,
            properties={
                "proto": "1",
                "server_id": self.cfg.server.server_id,
                "tls": "1",
            },
            server=f"{self.cfg.server.tls_name}.",
        )
        self.zc = Zeroconf()
        self.zc.register_service(self.info)

    def stop(self) -> None:
        if self.zc:
            try:
                self.zc.unregister_all_services()
            except Exception:
                pass
            self.zc.close()
            self.zc = None
            self.info = None

    def health(self) -> dict:
        return {
            "published": self.zc is not None,
            "service": SERVICE_TYPE,
            "tls_name": self.cfg.server.tls_name if self.cfg.server else None,
        }
