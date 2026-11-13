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
| LuaJIT | PUC Lua 5.1.5 (`lua/lua51/`) | The vendored LuaJIT 2.0.5 generates machine code with dynasm. It has no WebAssembly backend. The game's C API and scripts are Lua 5.1, so bots and levelgens still run. |
| Threads | `-DNO_THREADS=ON` (`TNL_NO_THREADS`) | pthreads need `SharedArrayBuffer` and cross-origin isolation headers. One-shot threads run inline. `DatabaseAccessThread` drains its queue once instead of looping forever, which would lock the tab. |
| Master server | `Game::checkConnectionToMaster` returns immediately | The master protocol is raw UDP. Browser sockets are WebSockets, and a hostname lookup cannot block the main thread. |
| UDP / internet play | Not implemented | `connectLocal` covers a self-hosted game. Real UDP does not exist in the browser. Emscripten's socket layer can open a datagram socket, but it is a WebSocket stand-in, not a peer. |
| Audio (OpenAL, ALURE, Vorbis, ModPlug, Speex) | `BF_NO_AUDIO` | No Emscripten port is wired for this stack. The existing no-audio stubs (the dedicated-server path) are used. Voice chat goes with it. |
| Discord RPC | `DISCORD=OFF` | The RPC transport is a local socket. |
| Test suite | Not built | gtest's pthread setup fights the single-threaded build. Native tests are unchanged. |

## Still native

`if(EMSCRIPTEN)` is checked before the UNIX/Linux platform file, so a normal `cmake .. && make` does not see these flags, Lua 5.1, or the main-loop change.
