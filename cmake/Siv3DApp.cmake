# -----------------------------------------------------------------------------
# アプリ側の CMakeLists.txt で、project() の後に include する (使い方は Siv3DAppPrelude.cmake を参照)。
#
# OpenSiv3D (Siv3D ライブラリ・SDL3・movie-player など) をサブディレクトリとして取り込み、
# siv3d_add_app() を使えるようにする。
# -----------------------------------------------------------------------------

include_guard(GLOBAL)

if(NOT SIV3D_SOURCE_DIR)
  message(FATAL_ERROR "Siv3DApp.cmake: include Siv3DAppPrelude.cmake before project()")
endif()

# macOS: Siv3D の Objective-C++ ソース (ゴミ箱・ファイルダイアログ補助) のため、トップレベルで有効にしておく
if(APPLE)
  enable_language(OBJCXX)
endif()

add_subdirectory("${SIV3D_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/siv3d")
