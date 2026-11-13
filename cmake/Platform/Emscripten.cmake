# Emscripten / WebAssembly.
#
# Included instead of Linux.cmake when the emcmake toolchain is active.
# wasm-ld does not accept --as-needed, and the browser has no libdl, so none
# of the native Linux link flags belong here.

# Ports and WebGL are selected with emcc flags. They must be on the compile
# line (headers) and the link line (libraries, preload, shell).
# Ogg/Vorbis decode menu.ogg and credits.ogg. OpenAL and the websocket
# bridge are link-only libraries, added below.
set(EMSCRIPTEN_PORT_FLAGS "-sUSE_SDL=2 -sUSE_LIBPNG=1 -sUSE_ZLIB=1 -sUSE_OGG=1 -sUSE_VORBIS=1 -sUSE_MODPLUG=1")

# C++ exceptions: Lua startup catches LuaException. Lua itself still uses
# setjmp/longjmp, which the default SUPPORT_LONGJMP provides.
set(EMSCRIPTEN_COMPILE_FLAGS "${EMSCRIPTEN_PORT_FLAGS} -fexceptions")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${EMSCRIPTEN_COMPILE_FLAGS}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${EMSCRIPTEN_COMPILE_FLAGS}")

# Assets are mounted at /bitfighter, which getExecutableDir() returns.
# The shell passes -name so the first screen is the main menu.
set(EMSCRIPTEN_LINK_FLAGS
	"${EMSCRIPTEN_PORT_FLAGS} -fexceptions \
	-sFULL_ES2=1 -sMIN_WEBGL_VERSION=1 -sMAX_WEBGL_VERSION=2 \
	-sALLOW_MEMORY_GROWTH=1 -sINITIAL_MEMORY=134217728 -sSTACK_SIZE=5242880 \
	-sFORCE_FILESYSTEM=1 \
	-lopenal -lwebsocket.js \
	--shell-file ${CMAKE_SOURCE_DIR}/cmake/emscripten/shell.html \
	--preload-file ${CMAKE_SOURCE_DIR}/resource@/bitfighter"
)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${EMSCRIPTEN_LINK_FLAGS}")


## Sub-project configuration

function(BF_PLATFORM_SET_EXTRA_SOURCES)
	# Do nothing!
endfunction()


function(BF_PLATFORM_SET_EXTRA_LIBS)
	# libm is in the toolchain. There is no libdl.
	set(EXTRA_LIBS m PARENT_SCOPE)
endfunction()


function(BF_PLATFORM_APPEND_LIBS)
	# Do nothing!
endfunction()


function(BF_PLATFORM_ADD_DEFINITIONS)
	add_definitions(-iquote ${CMAKE_SOURCE_DIR}/zap)
endfunction()


function(BF_PLATFORM_SET_EXECUTABLE_NAME)
	# Do nothing!
endfunction()


function(BF_PLATFORM_SET_TARGET_PROPERTIES targetName)
	# A .html suffix makes emcc emit html + js + wasm (+ the preloaded .data).
	set_target_properties(${targetName} PROPERTIES SUFFIX ".html")
endfunction()


function(BF_PLATFORM_SET_TARGET_OTHER_PROPERTIES targetName)
	# Do nothing
endfunction()


function(BF_PLATFORM_POST_BUILD_INSTALL_RESOURCES targetName)
	# Assets are inside the preloaded data file. Nothing to symlink.
endfunction()


function(BF_PLATFORM_INSTALL targetName)
	# Do nothing
endfunction()


function(BF_PLATFORM_BUNDLE_DEPENDENCIES targetName)
	# Do nothing
endfunction()


function(BF_PLATFORM_CREATE_PACKAGES targetName)
	# Do nothing!
endfunction()
