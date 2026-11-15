# -----------------------------------------------------------------------------
# Siv3D (SDL3 版) の vcpkg 設定。project() より前に include する。
#
# - OpenSiv3D 本体のトップレベル CMakeLists.txt と、アプリ側 (cmake/Siv3DAppPrelude.cmake 経由) の
#   両方から使う。vcpkg はトップレベルの project() の時点で manifest を処理するため、
#   アプリから OpenSiv3D を add_subdirectory する場合も、ここでの設定はアプリ側で先に行う必要がある。
# - SIV3D_SOURCE_DIR (OpenSiv3D のルート) を設定してから include すること。
# -----------------------------------------------------------------------------

if(NOT SIV3D_SOURCE_DIR)
  message(FATAL_ERROR "Siv3DVcpkg.cmake: SIV3D_SOURCE_DIR is not set")
endif()

# 動画のオプション (vcpkg の manifest feature と連動する)
# WebM (VP8 / VP9 + Vorbis / Opus) を movie-player (https://github.com/wamsoft/movie-player) で再生する
option(SIV3D_WITH_MOVIEPLAYER "Play WebM videos with movie-player (libvpx)" ON)
# Linux: opencv4 に FFmpeg (LGPL、共有ライブラリで同梱) を組み込み、MP4 などを VideoReader で開けるようにする。
# OFF にすると FFmpeg を一切含まない (Linux で開ける動画は WebM だけになる)。Windows / macOS は OS の機能を使うので影響しない
option(SIV3D_WITH_FFMPEG "Linux: build opencv4 with FFmpeg (LGPL, bundled as shared libraries)" ON)

if(SIV3D_WITH_MOVIEPLAYER AND NOT ("movieplayer" IN_LIST VCPKG_MANIFEST_FEATURES))
  list(APPEND VCPKG_MANIFEST_FEATURES "movieplayer")
endif()
if(SIV3D_WITH_FFMPEG AND NOT ("ffmpeg" IN_LIST VCPKG_MANIFEST_FEATURES))
  list(APPEND VCPKG_MANIFEST_FEATURES "ffmpeg")
endif()

# 依存の一覧は OpenSiv3D の vcpkg.json を使う (アプリ側に複製しない)
if(NOT DEFINED VCPKG_MANIFEST_DIR)
  set(VCPKG_MANIFEST_DIR "${SIV3D_SOURCE_DIR}" CACHE PATH "vcpkg manifest directory (OpenSiv3D's vcpkg.json)")
endif()

# overlay triplet (macOS の ANGLE を dylib に、Linux の FFmpeg を共有ライブラリにする)
if(NOT DEFINED VCPKG_OVERLAY_TRIPLETS)
  set(VCPKG_OVERLAY_TRIPLETS "${SIV3D_SOURCE_DIR}/cmake/vcpkg-triplets" CACHE PATH "vcpkg overlay triplets")
endif()

# プリセットを使わずに configure した場合の既定値 (CMakePresets.json の各プリセットと同じ)
if(NOT CMAKE_TOOLCHAIN_FILE AND DEFINED ENV{VCPKG_ROOT})
  file(TO_CMAKE_PATH "$ENV{VCPKG_ROOT}" _siv3d_vcpkg_root)
  set(CMAKE_TOOLCHAIN_FILE "${_siv3d_vcpkg_root}/scripts/buildsystems/vcpkg.cmake" CACHE FILEPATH "vcpkg toolchain")
endif()

if(NOT DEFINED VCPKG_TARGET_TRIPLET)
  if(CMAKE_HOST_WIN32)
    set(VCPKG_TARGET_TRIPLET "x64-windows-static" CACHE STRING "")
    set(VCPKG_CRT_LINKAGE "static" CACHE STRING "")
    set(VCPKG_LIBRARY_LINKAGE "static" CACHE STRING "")
    if(DEFINED _siv3d_vcpkg_root AND NOT DEFINED VCPKG_CHAINLOAD_TOOLCHAIN_FILE)
      set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${_siv3d_vcpkg_root}/scripts/toolchains/windows.cmake" CACHE FILEPATH "")
    endif()
  elseif(CMAKE_HOST_APPLE)
    if(CMAKE_OSX_ARCHITECTURES STREQUAL "arm64" OR (NOT CMAKE_OSX_ARCHITECTURES AND CMAKE_HOST_SYSTEM_PROCESSOR STREQUAL "arm64"))
      set(VCPKG_TARGET_TRIPLET "arm64-osx" CACHE STRING "")
    else()
      set(VCPKG_TARGET_TRIPLET "x64-osx" CACHE STRING "")
    endif()
  else()
    set(VCPKG_TARGET_TRIPLET "x64-linux" CACHE STRING "")
    set(VCPKG_CRT_LINKAGE "dynamic" CACHE STRING "")
    set(VCPKG_LIBRARY_LINKAGE "static" CACHE STRING "")
    if(DEFINED _siv3d_vcpkg_root AND NOT DEFINED VCPKG_CHAINLOAD_TOOLCHAIN_FILE)
      set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${_siv3d_vcpkg_root}/scripts/toolchains/linux.cmake" CACHE FILEPATH "")
    endif()
  endif()
endif()

if(NOT DEFINED CMAKE_CXX_STANDARD)
  set(CMAKE_CXX_STANDARD 23)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
endif()
