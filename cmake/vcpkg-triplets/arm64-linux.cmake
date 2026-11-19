set(VCPKG_TARGET_ARCHITECTURE arm64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# [Siv3D] Linux aarch64 (Retroid Pocket Mini など、Debian trixie arm64)。
# x86_64 ホストからのクロスビルドでは、vcpkg の linux.cmake が aarch64-linux-gnu-gcc を選ぶ
# (steamdev の deckbuild-rpmini イメージ。tools/siv3d_app.py --target rpmini)。
#
# ffmpeg だけ共有ライブラリにする理由は x64-linux.cmake と同じ (LGPL。exe の隣に同梱して
# $ORIGIN の DT_RPATH で解決し、RUNPATH は付けない)。
if(PORT STREQUAL "ffmpeg")
    set(VCPKG_LIBRARY_LINKAGE dynamic)
    set(VCPKG_FIXUP_ELF_RPATH OFF)
endif()
