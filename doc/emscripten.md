# Bitfighter in the browser (Emscripten proof of concept)

This is a minimal WebAssembly build of the Bitfighter client. Native Linux, Windows, and macOS builds are unchanged: every difference is behind `if(EMSCRIPTEN)` or `#ifdef __EMSCRIPTEN__`.

It loads the main menu and can host a local game (the client and server in the same page talk through `NetConnection::connectLocal`, not UDP). It does not speak to the master server, other browsers, or Discord, and it plays no audio.

## What you need

- [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) (tested with 7.0.0)
- CMake 3.10+ (invoked via `emcmake`)
- Python 3, only to serve the files

```
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh
```

## Build

From the repository root, with `emcc` on `PATH`:

```
mkdir -p build/emscripten
cd build/emscripten
emcmake cmake ../.. -DCMAKE_BUILD_TYPE=Release
emmake make -j$(nproc)
```

`emcmake` points CMake at Emscripten's toolchain. `emmake` is not strictly required for this Makefile generator, but it keeps `emcc` available to the port builds.

The link writes these files into `exe/`:

- `bitfighter.html`
- `bitfighter.js`
- `bitfighter.wasm`
- `bitfighter.data` (the `resource/` tree, preloaded at `/bitfighter`)

## Run

The page has to be served over HTTP. Opening the html file from disk will not load the wasm.

```
cd exe
python3 -m http.server 8080
```

Open `http://127.0.0.1:8080/bitfighter.html`.

A copy of this build is published from the `emscripten-poc` branch by `.github/workflows/emscripten-pages.yml` to <https://elementc.github.io/bitfighter/>. That workflow runs only on pushes to `emscripten-poc` (and when started by hand).

## UDP relay

Browsers cannot open raw UDP sockets. Emscripten's `websocket_to_posix_proxy` (`-sPROXY_POSIX_SOCKETS`) does bridge POSIX sockets, but it waits on a futex and needs pthreads plus `SharedArrayBuffer`. GitHub Pages does not send the cross-origin isolation headers that requires, and that mode also fights `emscripten_set_main_loop`. The browser build therefore uses a small relay, `tools/udp-ws-relay/relay.py`.

One WebSocket carries every datagram. Each browser socket is a real UDP socket on the relay. The game code changes only `Socket` send/receive when a relay URL is set. With no URL, sockets stay on the failing POSIX path and a local game still uses `connectLocal`.

```
python3 tools/udp-ws-relay/relay.py 28080
```

Open the page as `http://127.0.0.1:8080/bitfighter.html?relay=ws://127.0.0.1:28080`. The query parameter wins. `Module.relay` in the shell is the fallback. The hosted page is HTTPS, so a relay it can actually reach has to be `wss://` (put TLS in front of the Python process). An open `ws://` URL from that page is mixed content and the browser blocks it.

Outbound destination ports default to `25955` (master) and `28000` (game). Override with `RELAY_ALLOW_PORTS=25955,28000,28001` or `RELAY_ALLOW_ANY=1`. The default allowlist is intentional: without it the process is an open UDP proxy. Do not put an unrestricted relay on the public internet.

```
docker build -t bitfighter-udp-ws-relay tools/udp-ws-relay
docker run --rm --network host -e RELAY_ALLOW_PORTS=25955,28000 bitfighter-udp-ws-relay
```

What the relay host needs:

- A TCP port for the WebSocket, and TLS if browsers load the HTTPS Pages build.
- Outbound UDP, and a NAT that accepts the replies to those sockets. Client-to-server play only needs that.
- For other people to send *to* the browser (the browser hosting a public game, or an arranged peer connection), the relay's UDP ports have to be reachable from the internet. `--network host` is the straightforward way.

What works: the client talks to the real master at `107.172.71.173:25955` (the hostname entries are skipped, because Emscripten's `getaddrinfo` returns a fake address) and joins a server on an allowed port. Tested against a private `bitfighterd` on `127.0.0.1:28000` with `-serverpassword`.

What does not: arranged (NAT punch) connections. The master tells the client to send to the other peer's ephemeral port, and the default allowlist drops that. `RELAY_ALLOW_ANY=1` lets those packets through, on a host whose UDP ports are public. Two browsers still only see each other as the relay's address, not their own.

The shell passes `-name WebPilot`, so the first screen is the main menu rather than nickname entry. Click the canvas, then:

- Click **HOST GAME**, then **START HOSTING** (or press Enter on that row)
- `WASD` moves, the mouse aims and fires, `Esc` opens the in-game menu
- `H` is the global help binding, not the host shortcut

`ROBOTS` on the host menu is where bot counts live. The startup log may say the UDP socket failed to set its buffer sizes. That is the browser socket layer. A local game still connects through `NetConnection::connectLocal`.

To see the nickname screen instead, remove `arguments: ['-name', 'WebPilot']` from `cmake/emscripten/shell.html` and rebuild.

## Main loop

`dedicatedServerLoop()` is `for(;;) idle();` on native targets. On Emscripten it is `emscripten_set_main_loop(bitfighter_emscripten_main_loop, 0, 1)`, so the browser calls the same `idle()` from `requestAnimationFrame`. The `1` keeps `main()` from returning, which matches the native loop. `Platform::sleep()` is a no-op here because a blocking sleep cannot yield to the browser without Asyncify.

Asyncify was rejected. It rewrites every function that can reach `Platform::sleep()`, makes the wasm much larger, and conflicts with the C++ exceptions used while Lua is starting up.

## Disabled or stubbed

| Piece | What the browser build does | Why |
| --- | --- | --- |
| LuaJIT | Official Lua 5.1.5, downloaded by CMake at configure time | The vendored LuaJIT 2.0.5 generates machine code with dynasm and has no WebAssembly backend. The game's C API and scripts are Lua 5.1. The tarball is `https://www.lua.org/ftp/lua-5.1.5.tar.gz` pinned by SHA256, not copied into the git tree. |
| Threads | `-DNO_THREADS=ON` (`TNL_NO_THREADS`) | pthreads need `SharedArrayBuffer` and cross-origin isolation headers. One-shot threads run inline. `DatabaseAccessThread` drains its queue once instead of looping forever, which would lock the tab. |
| Master server | `Game::checkConnectionToMaster` returns immediately | The master protocol is raw UDP. Browser sockets are WebSockets, and a hostname lookup cannot block the main thread. |
| UDP / internet play | Not implemented | `connectLocal` covers a self-hosted game. Real UDP does not exist in the browser. Emscripten's socket layer can open a datagram socket, but it is a WebSocket stand-in, not a peer. |
| Audio | OpenAL (`-lopenal`) and in-tree ALURE, with the Ogg/Vorbis ports | Sound effects are wav files loaded straight into OpenAL. Menu and credits music are ogg. A click or key resumes the WebAudio context (autoplay policy). |
| Voice chat (Speex) | `BF_NO_VOICECHAT` | There is no Speex port and no Speex sources in the tree. |
| Tracker music (ModPlug, `.xm`) | Not decoded | No ModPlug port and no ModPlug sources. `menu.ogg` and `credits.ogg` still play. |
| Discord RPC | `DISCORD=OFF` | The RPC transport is a local socket. |
| Test suite | Not built | gtest's pthread setup fights the single-threaded build. Native tests are unchanged. |

## Still native

`if(EMSCRIPTEN)` is checked before the UNIX/Linux platform file, so a normal `cmake .. && make` does not see these flags, Lua 5.1, or the main-loop change.
