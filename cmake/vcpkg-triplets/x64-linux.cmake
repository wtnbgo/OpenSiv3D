set(VCPKG_TARGET_ARCHITECTURE x64)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)

set(VCPKG_CMAKE_SYSTEM_NAME Linux)

# [Siv3D] ffmpeg (opencv4 の VideoCapture が使う) だけは共有ライブラリで作る。
# ffmpeg は LGPL (gpl / nonfree / version3 feature は使わない) なので、利用者が差し替えられるよう
# 静的リンクせず、libavcodec / libavformat / libavutil / libswresample / libswscale の .so を
# 実行ファイルの隣に同梱して $ORIGIN (DT_RPATH) で解決する。
#
# vcpkg が既定で付ける RUNPATH ($ORIGIN) は付けない (VCPKG_FIXUP_ELF_RPATH OFF)。
# .so が RUNPATH を持つと、その .so の依存 (libavcodec → libswresample など) の解決に exe の
# DT_RPATH が使われず、LD_LIBRARY_PATH (Steam 起動時は Steam ランタイムや /usr/lib が先頭) の
# 後に探されるため、SteamOS 側の同じ soname の ffmpeg が読まれうる。RUNPATH を持たなければ
# exe の DT_RPATH ($ORIGIN) で同梱版が先に見つかる。
if(PORT STREQUAL "ffmpeg")
    set(VCPKG_LIBRARY_LINKAGE dynamic)
    set(VCPKG_FIXUP_ELF_RPATH OFF)
endif()
