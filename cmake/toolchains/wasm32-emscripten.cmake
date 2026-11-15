# -----------------------------------------------------------------------------
# Emscripten (Web 版) の toolchain: Emscripten.cmake + -pthread -fwasm-exceptions
#
# vcpkg は独自の VCPKG_CHAINLOAD_TOOLCHAIN_FILE を使うと triplet の VCPKG_C_FLAGS / VCPKG_CXX_FLAGS を
# ポートのビルドに反映しないので、Emscripten.cmake を include したうえで *_FLAGS_INIT にフラグを足す
# ラッパーを chainload する (vcpkg の依存と Siv3D 本体・アプリの両方で使う)。
#
# - -pthread: Siv3D はスレッドを使う (AsyncTask・VideoReader の先読み・movie-player 等)。wasm では
#   -pthread で atomics / bulk-memory を使う別 ABI になるので、依存ライブラリも同じフラグでビルドする
#   (配信時は SharedArrayBuffer のために COOP/COEP ヘッダが必要)
# - -fwasm-exceptions: Siv3D は C++ 例外を使う (Emscripten の既定は例外無効)
# -----------------------------------------------------------------------------

if(DEFINED ENV{EMSDK})
  file(TO_CMAKE_PATH "$ENV{EMSDK}" _siv3d_emsdk)
  set(_siv3d_emscripten_root "${_siv3d_emsdk}/upstream/emscripten")
elseif(DEFINED ENV{EMSCRIPTEN_ROOT})
  file(TO_CMAKE_PATH "$ENV{EMSCRIPTEN_ROOT}" _siv3d_emscripten_root)
else()
  message(FATAL_ERROR "EMSDK environment variable is required for the Web build")
endif()

include("${_siv3d_emscripten_root}/cmake/Modules/Platform/Emscripten.cmake")

string(APPEND CMAKE_C_FLAGS_INIT " -pthread -fwasm-exceptions")
string(APPEND CMAKE_CXX_FLAGS_INIT " -pthread -fwasm-exceptions")
string(APPEND CMAKE_EXE_LINKER_FLAGS_INIT " -pthread -fwasm-exceptions")
