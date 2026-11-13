# PUC Lua 5.1.5, built only for the Emscripten target.
# LuaJIT (the native scripting engine) has no WebAssembly backend. Bitfighter's
# scripts and C bindings are Lua 5.1, so this is the interpreter half of LuaJIT
# without the JIT. Sources are the stock lua.org 5.1.5 distribution (MIT);
# see COPYRIGHT. lua.c and luac.c, the standalone interpreters, are omitted.

file(GLOB LUA51_SOURCES ${CMAKE_CURRENT_SOURCE_DIR}/src/*.c)

add_library(lua STATIC ${LUA51_SOURCES})

target_include_directories(lua PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Generic ANSI build: no dlopen, no readline, no LUA_USE_LINUX.
# Emscripten's libc supplies the ISO functions Lua actually calls.
