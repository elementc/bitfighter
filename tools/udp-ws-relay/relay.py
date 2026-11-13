#!/usr/bin/env python3
"""WebSocket <-> UDP relay for the Bitfighter browser client.

Emscripten's posix socket proxy needs pthreads and SharedArrayBuffer.
GitHub Pages does not send the cross-origin headers those require, so the
browser build speaks this small framing protocol instead. One WebSocket
carries every datagram for that page. Each browser socket is a real UDP
socket on this process; replies come back on the same WebSocket.

Frame layout, big-endian, one WebSocket binary message per frame:

  BIND  client -> relay
    u8  1
    u32 sock_id     chosen by the client
    u16 port        0 picks an ephemeral port
  BOUND relay -> client
    u8  1
    u32 sock_id
    u16 port        the port actually bound

  SEND  client -> relay
    u8  2
    u32 sock_id
    u32 ipv4
    u16 port
    u16 length
    u8  payload[length]
  RECV  relay -> client
    u8  3
    u32 sock_id
    u32 ipv4        source address
    u16 port
    u16 length
    u8  payload[length]

Outbound destination ports are restricted. The default is the master-server
port and the default game port. Override with RELAY_ALLOW_PORTS=25955,28000
or RELAY_ALLOW_ANY=1. Do not publish an unrestricted relay: it is an open
UDP proxy.

Usage:
  python3 tools/udp-ws-relay/relay.py [port]
Default listen port is 28080.
"""

import asyncio
import os
import struct
import sys

BIND = 1
SEND = 2
RECV = 3
MAX_PAYLOAD = 8192

DEFAULT_PORTS = {25955, 28000}


def allowed_ports():
    if os.environ.get("RELAY_ALLOW_ANY") == "1":
        return None
    raw = os.environ.get("RELAY_ALLOW_PORTS", "25955,28000")
    ports = set()
    for part in raw.split(","):
        part = part.strip()
        if part:
            ports.add(int(part))
    return ports or DEFAULT_PORTS


ALLOW = allowed_ports()


def ws_accept(key: str) -> str:
    import base64
    import hashlib
    guid = b"258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
    digest = hashlib.sha1(key.encode("ascii") + guid).digest()
    return base64.b64encode(digest).decode("ascii")


class Client:
    def __init__(self, reader, writer):
        self.reader = reader
        self.writer = writer
        self.socks = {}  # sock_id -> (transport, protocol)

    def send_frame(self, payload: bytes):
        n = len(payload)
        header = bytearray()
        header.append(0x82)  # FIN + binary
        if n < 126:
            header.append(n)
        elif n < 65536:
            header.append(126)
            header += struct.pack("!H", n)
        else:
            header.append(127)
            header += struct.pack("!Q", n)
        self.writer.write(bytes(header) + payload)

    async def read_exact(self, n):
        buf = bytearray()
        while len(buf) < n:
            chunk = await self.reader.read(n - len(buf))
            if not chunk:
                raise EOFError
            buf += chunk
        return bytes(buf)

    async def read_frame(self):
        b0, b1 = await self.read_exact(2)
        opcode = b0 & 0x0F
        masked = (b1 & 0x80) != 0
        length = b1 & 0x7F
        if length == 126:
            length = struct.unpack("!H", await self.read_exact(2))[0]
        elif length == 127:
            length = struct.unpack("!Q", await self.read_exact(8))[0]
        mask = await self.read_exact(4) if masked else b""
        data = bytearray(await self.read_exact(length)) if length else bytearray()
        if masked:
            for i in range(length):
                data[i] ^= mask[i % 4]
        return opcode, bytes(data)


class UDP(asyncio.DatagramProtocol):
    def __init__(self, client, sock_id):
        self.client = client
        self.sock_id = sock_id

    def datagram_received(self, data, addr):
        ip = struct.unpack("!I", bytes(map(int, addr[0].split("."))))[0] if ":" not in addr[0] else 0
        if ":" in addr[0]:
            return  # IPv6 replies are not part of this PoC
        frame = struct.pack("!BIIHH", RECV, self.sock_id, ip, addr[1], len(data)) + data
        try:
            self.client.send_frame(frame)
        except Exception:
            pass


async def bind_socket(client, sock_id, port):
    loop = asyncio.get_running_loop()

    def factory():
        return UDP(client, sock_id)

    transport, _protocol = await loop.create_datagram_endpoint(
        factory, local_addr=("0.0.0.0", port)
    )
    bound = transport.get_extra_info("sockname")[1]
    client.socks[sock_id] = transport
    client.send_frame(struct.pack("!BIH", BIND, sock_id, bound))
    print(f"bind sock={sock_id} requested={port} actual={bound}", flush=True)


async def handle_message(client, payload):
    if not payload:
        return
    kind = payload[0]
    if kind == BIND and len(payload) >= 7:
        sock_id, port = struct.unpack("!IH", payload[1:7])
        if sock_id in client.socks:
            client.socks[sock_id].close()
            del client.socks[sock_id]
        await bind_socket(client, sock_id, port)
    elif kind == SEND and len(payload) >= 13:
        sock_id, ip, port, length = struct.unpack("!IIHH", payload[1:13])
        data = payload[13:13 + length]
        if ALLOW is not None and port not in ALLOW:
            print(f"drop send to port {port} (not in RELAY_ALLOW_PORTS)", flush=True)
            return
        transport = client.socks.get(sock_id)
        if transport is None or len(data) != length:
            return
        host = ".".join(str((ip >> shift) & 0xFF) for shift in (24, 16, 8, 0))
        print(f"send sock={sock_id} {host}:{port} {len(data)} bytes", flush=True)
        transport.sendto(data, (host, port))


async def connection(reader, writer):
    peer = writer.get_extra_info("peername")
    try:
        req = b""
        while b"\r\n\r\n" not in req:
            chunk = await reader.read(1024)
            if not chunk:
                return
            req += chunk
            if len(req) > 8192:
                return
        text = req.decode("latin1", "replace")
        if "Upgrade: websocket" not in text and "Upgrade: WebSocket" not in text:
            writer.close()
            return
        key = ""
        for line in text.split("\r\n"):
            if line.lower().startswith("sec-websocket-key:"):
                key = line.split(":", 1)[1].strip()
        if not key:
            writer.close()
            return
        accept = ws_accept(key)
        writer.write(
            (
                "HTTP/1.1 101 Switching Protocols\r\n"
                "Upgrade: websocket\r\n"
                "Connection: Upgrade\r\n"
                f"Sec-WebSocket-Accept: {accept}\r\n\r\n"
            ).encode("ascii")
        )
        await writer.drain()
        client = Client(reader, writer)
        print(f"websocket from {peer}", flush=True)
        while True:
            opcode, data = await client.read_frame()
            if opcode == 0x8:
                break
            if opcode == 0x9:  # ping
                # pong, unmasked
                n = len(data)
                writer.write(bytes([0x8A, n]) + data)
                continue
            if opcode == 0x2 or opcode == 0x0:
                await handle_message(client, data)
    except (asyncio.IncompleteReadError, EOFError, ConnectionResetError):
        pass
    finally:
        if "client" in dir():
            for transport in client.socks.values():
                transport.close()
        writer.close()
        print(f"closed {peer}", flush=True)


async def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 28080
    server = await asyncio.start_server(connection, "0.0.0.0", port)
    ports = "any" if ALLOW is None else ",".join(str(p) for p in sorted(ALLOW))
    print(f"udp-ws-relay listening on ws://0.0.0.0:{port}/  allow_ports={ports}", flush=True)
    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    asyncio.run(main())
