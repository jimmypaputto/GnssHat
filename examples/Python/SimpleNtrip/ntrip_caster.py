# Jimmy Paputto 2026

#
# Simple NTRIP Caster
#
# A lightweight NTRIP v2.0 caster that broadcasts RTCM3 correction
# data to connected NTRIP clients. Designed to run on a Raspberry Pi
# alongside the RTK Base station script (base.py).
#
# No authentication — open for any client on the local network.
#

import socketserver
import threading
import logging

log = logging.getLogger("ntrip_caster")


class NtripCaster:
    """
    Simple single-mountpoint NTRIP v2.0 caster.

    Usage:
        caster = NtripCaster("0.0.0.0", 2101, "MY_RTK_BASE")
        caster.start()
        ...
        caster.feed(rtcm3_frames)  # called from base station loop
        ...
        caster.stop()
    """

    def __init__(self, host: str, port: int, mountpoint: str):
        self._host = host
        self._port = port
        self._mountpoint = mountpoint
        self._clients: list[_ClientHandler] = []
        self._clients_lock = threading.Lock()
        self._server: _NtripTCPServer | None = None
        self._thread: threading.Thread | None = None

    def start(self):
        """Start the NTRIP caster in a background thread."""
        self._server = _NtripTCPServer(
            (self._host, self._port),
            _ClientHandler,
            caster=self,
        )
        self._server.daemon_threads = True
        self._thread = threading.Thread(
            target=self._server.serve_forever,
            daemon=True,
        )
        self._thread.start()
        log.info(f"NTRIP caster listening on "
                 f"{self._host}:{self._port}/{self._mountpoint}")

    def stop(self):
        """Shut down the caster and disconnect all clients."""
        if self._server is not None:
            self._server.shutdown()
            self._server.server_close()
        with self._clients_lock:
            for client in self._clients:
                try:
                    client.request.close()
                except OSError:
                    pass
            self._clients.clear()
        log.info("NTRIP caster stopped.")

    def feed(self, frames: list[bytes]):
        """Broadcast RTCM3 frames to all connected clients."""
        data = b"".join(frames)
        if not data:
            return
        with self._clients_lock:
            dead = []
            for client in self._clients:
                try:
                    client.request.sendall(data)
                except (BrokenPipeError, ConnectionResetError, OSError):
                    dead.append(client)
            for client in dead:
                self._clients.remove(client)
                log.info(f"Client {client.client_address} disconnected "
                         f"(total: {len(self._clients)})")

    def register_client(self, handler: "_ClientHandler"):
        with self._clients_lock:
            self._clients.append(handler)
            log.info(f"Client {handler.client_address} connected "
                     f"(total: {len(self._clients)})")

    @property
    def client_count(self) -> int:
        with self._clients_lock:
            return len(self._clients)

    @property
    def mountpoint(self) -> str:
        return self._mountpoint


class _NtripTCPServer(socketserver.ThreadingTCPServer):
    allow_reuse_address = True

    def __init__(self, server_address, RequestHandlerClass, *, caster):
        self.caster: NtripCaster = caster
        super().__init__(server_address, RequestHandlerClass)


class _ClientHandler(socketserver.BaseRequestHandler):
    """Handles incoming NTRIP client connections."""

    server: _NtripTCPServer

    def handle(self):
        caster = self.server.caster
        try:
            raw = self.request.recv(4096)
            if not raw:
                return
            request_line = raw.decode("ascii", errors="replace")
        except (ConnectionResetError, OSError):
            return

        # Parse the HTTP request line: "GET /MOUNTPOINT HTTP/1.1\r\n..."
        lines = request_line.split("\r\n")
        if not lines:
            return

        parts = lines[0].split()
        if len(parts) < 2 or parts[0] != "GET":
            self._send_response("405 Method Not Allowed",
                                "Only GET is supported.\r\n")
            return

        path = parts[1].lstrip("/")

        # Empty path or "/" → return sourcetable
        if not path:
            self._send_sourcetable(caster)
            return

        # Check mountpoint
        if path != caster.mountpoint:
            self._send_response("404 Not Found",
                                f"Mountpoint '{path}' not found.\r\n")
            return

        # Accept the client — send ICY 200 OK (NTRIP v2.0)
        try:
            self.request.sendall(
                b"ICY 200 OK\r\n"
                b"Content-Type: gnss/data\r\n"
                b"Cache-Control: no-store\r\n"
                b"\r\n"
            )
        except OSError:
            return

        # Register and block until the connection dies
        caster.register_client(self)

        # Keep the handler alive — data is pushed via feed()
        try:
            while True:
                # Wait for client disconnect (recv returns b"" on close)
                data = self.request.recv(1024)
                if not data:
                    break
        except (ConnectionResetError, OSError):
            pass

    def _send_response(self, status: str, body: str):
        try:
            resp = (f"HTTP/1.1 {status}\r\n"
                    f"Content-Type: text/plain\r\n"
                    f"Content-Length: {len(body)}\r\n"
                    f"\r\n"
                    f"{body}")
            self.request.sendall(resp.encode("ascii"))
        except OSError:
            pass

    def _send_sourcetable(self, caster: NtripCaster):
        entry = (f"STR;{caster.mountpoint};;"
                 f"RTCM 3.3;;"
                 f"2;GPS+GLO+GAL+BDS;"
                 f"SNIP;POL;0.00;0.00;"
                 f"0;0;sNTRIP;none;N;N;;\r\n")
        body = entry + "ENDSOURCETABLE\r\n"
        try:
            resp = ("ICY 200 OK\r\n"
                    "Content-Type: gnss/sourcetable\r\n"
                    f"Content-Length: {len(body)}\r\n"
                    "\r\n"
                    + body)
            self.request.sendall(resp.encode("ascii"))
        except OSError:
            pass
