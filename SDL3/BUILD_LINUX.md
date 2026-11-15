# OpenSiv3D (SDL3 版) Linux ビルド

SDL3 版 OpenSiv3D の Linux ビルドは、Valve の **Steam Linux Runtime 3.0 "sniper" SDK**
(Debian 11 / glibc 2.31) コンテナでビルドしたものを基準とする。
新しい glibc のディストリビューション (Ubuntu 24.04 以降など) で直接ビルドしたバイナリは
古い環境で動かないため、配布用ビルドの確認には使わない。

sniper SDK でビルドしたバイナリは SteamOS (ネイティブ)・Steam Linux Runtime コンテナ内・
glibc 2.31 以降の一般的なディストリビューションで動く。

## ビルド環境

[steamdev](https://github.com/wamsoft/steamdev) リポジトリの `deckbuild/` を使う
(吉里吉里Z の Linux ビルドと同じ環境)。

- ベース: `registry.gitlab.steamos.cloud/steamrt/sniper/sdk`
- コンパイラ: SDK 同梱の gcc-14 (glibc ターゲットは 2.31 のまま)
- 追加: CMake 3.31 / vcpkg (`VCPKG_ROOT=/opt/vcpkg`) / nasm・yasm
- 実行: Linux ホストまたは WSL2 の docker

```bash
# 初回のみ: ビルドイメージ作成 (deckbuild-sniper)
<steamdev>/deckbuild/deckbuild.sh image
```

## ビルド手順

プリセットは `x64-linux` (Ninja Multi-Config / vcpkg triplet `x64-linux`)。
サンプルアプリ `Siv3DApp` (`SDL3/App/Main.cpp`) は `SIV3D_BUILD_SAMPLE=ON` のときだけビルドされる。

```bash
# configure + build + install (既定: Release)
PRESET=x64-linux CMAKEOPT='-DSIV3D_BUILD_SAMPLE=ON' \
    <steamdev>/deckbuild/deckbuild.sh -s ~/OpenSiv3D all

# 個別ステップ / コンテナ内シェル / ビルドツリー破棄
<steamdev>/deckbuild/deckbuild.sh -s ~/OpenSiv3D configure|build|install|shell|clean
```

- 初回の configure は vcpkg の依存 (boost / opencv4 / curl など) のビルドで時間がかかる。
  バイナリキャッシュは docker volume (`deckbuild-cache`) に残るので 2 回目以降は速い
- configure は FetchContent で SDL3 を取得するのでネットワークが必要
- ビルドツリー `build/x64-linux` は docker named volume 上にあり、ホストからは見えない
- `Siv3DApp` 本体はビルドツリーとは別に `SDL3/App/Release/` (ソース側) にも出力される
- install 先は `bin/x64-linux/Release/`。コンテナは root で動くので、ホストで触る前に所有者を戻す:

```bash
docker run --rm -v $PWD/bin:/b -v $PWD/SDL3/App:/a deckbuild-sniper \
    chown -R $(id -u):$(id -g) /b /a/Release
```

### install 先の構成

```
bin/x64-linux/Release/
  Siv3DApp                 実行ファイル (RPATH = $ORIGIN)
  libSDL3.so.0.x.y         同梱 SDL3
  libSDL3.so.0 -> libSDL3.so.0.x.y
  libavcodec.so.61 / libavformat.so.61 / libavutil.so.59 / libswresample.so.5 / libswscale.so.8
                           同梱 ffmpeg (LGPL、共有ライブラリ。soname のリンクと実体)
  resources/engine/        エンジン資材 (フォント / シェーダ (essl) / テクスチャ / サウンドフォント)
  example/                 サンプル資材
  licenses/ffmpeg.txt      ffmpeg のライセンス文 (LGPL 2.1)
  licenses/ffmpeg-source.txt  ffmpeg のバージョン・ソースの入手先・vcpkg のポート (configure 時に生成)
  lib/ include/            Siv3D 静的ライブラリと公開ヘッダ (アプリを別途ビルドする場合用)
```

`resources/` は `Resource()` が「実行ファイルのフォルダ/resources/」から読む。
`example/` はカレントディレクトリ相対で読むので、**install 先のフォルダに移動してから起動する**:

```bash
cd bin/x64-linux/Release && ./Siv3DApp
```

映像ドライバは SDL3 が自動選択する (Wayland セッションでは wayland。
`SDL_VIDEO_DRIVER=x11` / `wayland` で切り替え可能)。

## バイナリの合格基準

コンテナ内 (`deckbuild.sh shell` または `deckbuild.sh run '<コマンド>'`) で確認する。

```bash
cd bin/x64-linux/Release

# 要求 glibc シンボルの最大バージョン — 2.31 以下
objdump -T Siv3DApp | grep -o "GLIBC_[0-9.]*" | sort -Vu | tail -1

# GLIBCXX 依存が出ないこと (libstdc++ は静的リンク)
objdump -T Siv3DApp | grep -o "GLIBCXX_[0-9.]*" | sort -Vu | tail -1

# 動的リンクする .so と RPATH — RPATH が $ORIGIN (RUNPATH ではないこと)
readelf -d Siv3DApp | grep -E "NEEDED|RPATH|RUNPATH"

# unwinder を exe に複製していないこと (libgcc_s.so.1 から解決)
nm Siv3DApp | grep -c " [Tt] _Unwind_RaiseException"     # 0
objdump -T Siv3DApp | grep -o "GCC_[0-9.]*" | sort -Vu | tail -1   # GCC_4.2.0

# 同梱 SDL3 / ffmpeg も同じ基準。ffmpeg の .so は RPATH / RUNPATH を持たないこと
for f in libSDL3.so.0 libav*.so.?? libsw*.so.?; do
  echo "$f $(objdump -T $f | grep -o "GLIBC_[0-9.]*" | sort -Vu | tail -1) $(readelf -d $f | grep -E "RPATH|RUNPATH")"
done
```

実測 (2026-11-15, sniper SDK + gcc-14, Release):

| 項目 | Siv3DApp | libSDL3.so.0 |
|---|---|---|
| GLIBC 最大 | 2.29 | 2.30 |
| GLIBCXX 依存 | なし | なし |
| GCC_ シンボル最大 | 4.2.0 | - |
| RPATH | `$ORIGIN` (DT_RPATH) | - |
| `_Unwind_RaiseException` の定義 | 0 | - |

同梱 ffmpeg (7.1.1): GLIBC 最大 2.29 (libavformat は 2.28)、GLIBCXX 依存なし、RPATH / RUNPATH なし。

NEEDED: `libgcc_s.so.1 libpthread.so.0 libavformat.so.61 libavcodec.so.61 libswscale.so.8
libavutil.so.59 libdl.so.2 libSDL3.so.0 libm.so.6 libc.so.6 ld-linux-x86-64.so.2`
(同梱の SDL3 / ffmpeg 以外は glibc / libgcc のみ。gtk / gdk / gio / gobject / glib は含まない)。
音声ライブラリ (libpulse / libasound / libjack / PipeWire) は miniaudio が実行時に dlopen するので
NEEDED には出ない。

## Linux 固有の構成 (CMakeLists.txt)

Linux 固有の処理は `if(UNIX AND NOT APPLE)` で分岐している。

- **同梱 libSDL3 を DT_RPATH (`$ORIGIN`) で解決**: `--disable-new-dtags`。Steam から起動すると
  `LD_LIBRARY_PATH` の先頭に Steam ランタイムや `/usr/lib` が入り、RUNPATH (LD_LIBRARY_PATH の
  後に探される) ではシステム側の libSDL3 が勝ってしまうため。`LD_LIBRARY_PATH=.` は不要
- **libgcc_s.so.1 を直接リンク**: sniper SDK の gcc-14 は libstdc++ / libgcc を常に静的にリンクする
  (`-shared-libgcc` も効かない)。例外の unwinder を共有の `libgcc_s.so.1` に一本化し、
  共有ライブラリ側と複製を持ち合わないようにする (吉里吉里Z と同じ扱い)。
  libstdc++ は静的のままなので GLIBCXX 依存は増えない
- **SSE4.2 必須**: Linux x86_64 では `Siv3D` に `-msse4.2` (PUBLIC) を付ける。
  `Platform.hpp` は Linux では `__SSE4_2__` があるときだけネイティブの SSE 組み込み関数を使い、
  無い場合の simde 互換経路は gcc-14 の intrinsic ヘッダと衝突してコンパイルできない。
  **実行には SSE4.2 対応 CPU が必要** (2008 年以降の一般的な x86_64 CPU / Steam Deck は対応)
- **GTK3 / GIO / glib に依存しない**: ファイルダイアログは SDL3 (`SDL_ShowFileDialogWithProperties`。
  xdg-desktop-portal 経由、無ければ zenity)、ゴミ箱 (`FileSystem::Remove(path, AllowUndo::Yes)`) は
  freedesktop.org Trash 仕様の自前実装。ダイアログを出すには portal (GNOME / KDE / SteamOS の
  デスクトップモードには標準である) か zenity が必要
- **libuuid は SDK の静的ライブラリ** (`libuuid.a`) をリンク (stduuid の `uuid_generate`)
- **vcpkg.json**: Linux では opencv4 の default-features を切る。既定のままだと
  gtk / highgui 経由で gtk3・cairo・fontconfig 一式を vcpkg でビルドしようとし、
  sniper SDK では gperf のビルドで失敗する。glib も Linux では vcpkg から外す
- **動画 (VideoReader / VideoTexture) は opencv4 + ffmpeg**: Linux の opencv4 に `ffmpeg` feature を
  付け、ffmpeg は `avcodec / avformat / swresample / swscale` だけ (default-features なし。
  avdevice / avfilter は作らない) をビルドする。gpl / nonfree / version3 feature は使わないので
  **LGPL 2.1 以降**。H.264 / AAC なども ffmpeg 内蔵のデコーダーで再生できる
- **ffmpeg だけ共有ライブラリ** (overlay triplet `cmake/vcpkg-triplets/x64-linux.cmake`、プリセット
  `x64-linux` の `VCPKG_OVERLAY_TRIPLETS`): LGPL の再リンク要件を満たすため静的リンクしない。
  libav* / libsw* の .so を実行ファイルの隣に同梱し、exe の DT_RPATH (`$ORIGIN`) で解決する。
  ffmpeg の .so には vcpkg が既定で付ける RUNPATH を付けない (`VCPKG_FIXUP_ELF_RPATH OFF`):
  RUNPATH があると .so 同士の依存 (libavcodec → libswresample 等) に exe の DT_RPATH が使われず、
  Steam 起動時の `LD_LIBRARY_PATH` にある SteamOS 側の同じ soname の ffmpeg が読まれうるため。
  `LD_LIBRARY_PATH` に同じ soname の偽物を置いても、5 本とも同梱版が読まれることを確認済み
- **ffmpeg のビルド**: x86 の asm に nasm を使う (steamdev のイメージに入っている)。所要時間は
  ffmpeg 約 4 分 + opencv4 の再ビルド約 6 分 (overlay triplet の導入で triplet の ABI が変わるため、
  初回はバイナリキャッシュに無い依存も作り直しになる。全体で約 20 分)

## 既知の制限・注意点

- **ffmpeg (LGPL) の配布条件**: install 先 (とビルドツリーの実行ファイルの隣) の `licenses/` に
  ffmpeg のライセンス文 (`ffmpeg.txt`) と、ソースの入手先 (`ffmpeg-source.txt`。FFmpeg n7.1.1 の
  ソースと vcpkg のポート `ffmpeg` 7.1.1#6 のコミット) が自動で入る。アプリを配布するときは
  `licenses/` ごと同梱すること。.so を差し替えて動かせる形 (共有ライブラリ) なので、再リンクの要件も満たす
- **画面がスリープ中 (スクリーンセーバー動作中) の X11 では起動直後に止まる**:
  コンポジタがフレームを表示しないため、垂直同期付きの `SwapBuffers`
  (`X11_GL_SwapWindow` → Mesa DRI3) が返らない。Wayland では SDL3 がフレーム待ちに
  タイムアウトを持つので進む。画面を復帰させてから実行する
  (吉里吉里Z でも、画面スリープ中の Wayland 起動で SDL3 初期化が落ちる事例がある)
- ビルド後に `resources/` と `example/` が実行ファイルの隣 (`SDL3/App/Release/`) にも
  コピーされるので、ビルドツリーでも起動できる (`SDL3/App/Release` に移動してから起動)
- ゴミ箱: ホームと別のファイルシステム (USB メモリ・SD カードなど) では `<マウントポイント>/.Trash-<uid>` に入れる。
  ただし tmpfs の `/tmp`・`/dev/shm` などシステム内部のマウントでは、GIO の `g_file_trash` と同じく
  ゴミ箱に移さず `false` を返す (ファイルは残る)。そこに入れても GNOME などのゴミ箱 (GVfs) には
  表示されず元に戻せない (tmpfs なら再起動で消える) ため。判定は `/proc/self/mountinfo` の
  ファイルシステム種別 (tmpfs / proc / overlay など) とマウントポイント (`/`・`/tmp`・`/var` など) で行う
- sniper SDK イメージはローリング更新される。再現性を厳密にしたい場合は
  steamdev の Dockerfile の `FROM` をダイジェスト固定にする

## 依存ライブラリのライセンス

Linux の配布物 (`Siv3DApp` と同梱 .so) に入るものを、LGPL / GPL の観点で確認した結果
(2026-11-15、vcpkg の `share/*/copyright` を `Lesser|LGPL|GPL` で検索し、実際に `Siv3DApp` の
リンク行に載るライブラリ・sniper SDK から静的リンクするもの・実行時に dlopen するものを確認)。

| 対象 | 扱い | ライセンス | 判断 |
|---|---|---|---|
| ffmpeg 7.1.1 (avcodec / avformat / avutil / swresample / swscale) | 共有ライブラリで同梱 | **LGPL 2.1 以降** | 各 .so の `*_license()` 文字列が "LGPL version 2.1 or later"。configure に `--enable-gpl` / `--enable-nonfree` / `--enable-version3` も外部コーデックライブラリ (`--enable-lib*`) も無いことを、`libavcodec` に埋め込まれた configure 行で確認。ライセンス文とソースの入手先は `licenses/` に同梱 |
| freetype | 静的 | FTL / GPLv2 の選択制 | FTL を選べる (問題なし) |
| liblzma (xz) | 静的 | 0BSD | LGPL は getopt_long を持たない環境の CLI 用だけで、ライブラリには含まれない |
| zstd | 静的 | BSD / GPLv2 の選択制 | BSD を選べる |
| SDL3 (同梱 .so) | 共有 | zlib | hidapi は GPLv3 / BSD / HIDAPI の選択制で BSD を選べる |
| libuuid (sniper SDK の `libuuid.a`) | 静的 | BSD-3-Clause (util-linux の `libuuid/*`) | 問題なし |
| libstdc++ / libgcc の静的部分 (gcc-14) | 静的 | GPLv3 + GCC Runtime Library Exception | 例外により配布物にライセンス上の制約は及ばない |
| libgcc_s.so.1 / glibc | OS のものを使う (同梱しない) | GPLv3+例外 / LGPL 2.1 | 同梱しないので配布物には入らない |
| 音声 (libpulse / libasound / libjack / PipeWire)・libdecor / Wayland / X11 など | SDL3 / miniaudio が実行時に dlopen (同梱しない) | — | OS 側のライブラリ。install 先の .so は libSDL3 と ffmpeg の 5 本だけ |
| その他の vcpkg 静的ライブラリ (opencv4, libpng, libjpeg-turbo, libtiff, libwebp, giflib, harfbuzz, curl, OpenSSL 3, brotli, bzip2, zlib, ogg / vorbis / opus, quirc, boost) | 静的 | Apache-2.0 / BSD / MIT / zlib 系 | copyright に LGPL / GPL の記載なし |
| `Siv3D/src/ThirdParty`, `Siv3D/include/ThirdParty` | 静的 | — | LGPL / GPL の記載なし。SoundTouch (LGPL) は `NullSoundTouchFunctions` の空実装で、本体は含まれない |

結論: LGPL / GPL のもので配布物に入るのは ffmpeg (LGPL 2.1+、共有ライブラリ) だけ。

## 動作確認

2026-11-15、Ubuntu 26.04 (GNOME / Wayland, Mesa Intel Xe, OpenGL ES 3.2) のホストで
install 先の `Siv3DApp` を起動し、`SDL3/App/Main.cpp` のデモ (テクスチャ・絵文字・
MSDF フォント・GUI) が Wayland / X11 (XWayland, `SDL_VIDEO_DRIVER=x11`) とも描画されることを確認した。

ファイルダイアログ (`Dialog::OpenFile` / `OpenFiles` / `SaveFile` / `SelectFolder`) は、
xdg-desktop-portal の FileChooser を模したテスト用サービスと偽の zenity で、タイトル・フィルタ・
複数選択・フォルダ選択・初期フォルダ / 保存ファイル名・戻り値 (空白や日本語を含むパス) が
正しく受け渡されることを確認した。キャンセル時と、portal も zenity も無い環境では
`none` (空配列) が返り、落ちない。
ダイアログ表示中に SIGTERM を送ると、待機を打ち切って `none` で戻り、終了コード 0 で終了する
(SDL3 には表示中のダイアログを閉じる API が無いので、portal は D-Bus の切断で閉じ、
zenity は自プロセスの子だけを SIGKILL で閉じる。zenity は SIGTERM をブロックした状態で起動されるため)。
ゴミ箱 (`FileSystem::Remove(path, AllowUndo::Yes)`) はホームのゴミ箱
(`~/.local/share/Trash/files` と `info/*.trashinfo`) に入り、`gio trash --list` に表示され、
`gio trash --restore` (GNOME の「ゴミ箱」と同じ GVfs) で元に戻せることを確認した。
ユーザーがマウントしたドライブ (udisks で自動マウントした ext4、`/run/media/<user>/...`) では
`.Trash-<uid>` に入って GNOME のゴミ箱に表示され、元に戻せる。tmpfs の `/tmp`・`/dev/shm` では
`false` が返り、ファイルはそのまま残る。

音声は miniaudio が選ばれる (ログに `🎧 Audio backend: MiniAudio`)。PipeWire (pipewire-pulse) 環境で
libpulse が実行時に読み込まれ、MP3 (`example/test.mp3`, 103.42 秒) と MIDI (`example/midi/test.mid`,
205.70 秒) が読めて、再生位置が進む (2 秒後に posSec 2.0) ことを確認した。
動画は `example/video/river.mp4` が開け (1280x720, 29.97fps, 752 フレーム)、`readFrame` で
1280x720 の画像が得られることを確認した (Windows と同じ結果)。
