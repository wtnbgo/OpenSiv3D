# -----------------------------------------------------------------------------
# アプリ側の CMakeLists.txt で、project() より前に include する。
#
#   cmake_minimum_required(VERSION 3.21)
#   include("${SIV3D_BASE}/cmake/Siv3DAppPrelude.cmake")   # ← project() より前
#   project(MyGame LANGUAGES C CXX)
#   include("${SIV3D_BASE}/cmake/Siv3DApp.cmake")          # ← project() の後
#   siv3d_add_app(MyGame SOURCES src/Main.cpp CONFIG siv3d-app.json)
#
# vcpkg の toolchain・triplet・manifest (OpenSiv3D の vcpkg.json を使う)・overlay triplet・
# 動画のオプション (SIV3D_WITH_MOVIEPLAYER / SIV3D_WITH_FFMPEG) を設定する。
# vcpkg はトップレベルの project() の時点で依存をそろえるため、OpenSiv3D を add_subdirectory する
# アプリ側で先に設定しておく必要がある。
# -----------------------------------------------------------------------------

get_filename_component(SIV3D_SOURCE_DIR "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
set(SIV3D_SOURCE_DIR "${SIV3D_SOURCE_DIR}" CACHE INTERNAL "OpenSiv3D source directory")

include("${SIV3D_SOURCE_DIR}/cmake/Siv3DVcpkg.cmake")
