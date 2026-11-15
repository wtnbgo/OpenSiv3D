# OpenSiv3D (SDL3 版) Windows ビルド手順

SDL3 + OpenGL ES 3.0 バックエンドを Windows でビルドし、サンプルアプリ (`Siv3DApp.exe`) を動かす手順です。

依存ライブラリは vcpkg の静的ライブラリ (`x64-windows-static`、CRT も静的) としてリンクするので、配布時に必要なのは `Siv3DApp.exe` と `SDL3.dll`、それに `resources/` だけです。

## 必要なもの

| ツール | 確認済みバージョン |
|---|---|
| Windows | 11 (x64) |
| Visual Studio | 2026 (18) Professional、MSVC 14.51 (「C++ によるデスクトップ開発」ワークロード) |
| CMake | 4.3 (Visual Studio 同梱版。3.20 以上) |
| Ninja | 1.13 (Visual Studio 同梱版) |
| vcpkg | 環境変数 `VCPKG_ROOT` を設定しておく |

## ビルド

**x64 の開発者環境**で実行します。「x64 Native Tools Command Prompt」を開くか、`vcvars64.bat` を読み込んでから実行してください。

```bat
call "C:\Program Files\Microsoft Visual Studio\18\Professional\VC\Auxiliary\Build\vcvars64.bat"

cmake --preset x64-windows -DSIV3D_BUILD_SAMPLE=ON
cmake --build build/x64-windows --config Release
```

- 初回の configure では vcpkg が依存ライブラリ (OpenCV・Boost・FreeType・HarfBuzz など) をソースからビルドするため、時間がかかります。
- SDL3 は CMake の FetchContent で取得されます。
- `SIV3D_BUILD_SAMPLE=ON` を付けると `SDL3/App/Main.cpp` からサンプルアプリ `Siv3DApp` をビルドします (既定は OFF で、エンジン本体の `Siv3D.lib` だけをビルドします)。
- ジェネレータは Ninja Multi-Config です。`--config` に `Debug` / `RelWithDebInfo` / `Release` を指定できます。

### 開発者環境を混ぜない

Visual Studio が複数入っている環境では、**configure したときと同じバージョンの開発者環境でビルド**してください。

- `CMakeCache.txt` が古い VS (例: 2022) の `cl.exe` を指したまま、新しい VS の環境でビルドすると、STL のヘッダとコンパイラの組み合わせが合わず `STL1001` などのエラーになります。
- vcpkg の依存ライブラリが別の VS でビルドされていると、リンク時に `__std_*` 系の未解決シンボルが出ます。
- どちらの場合も、使う VS の開発者環境から `cmake --preset x64-windows --fresh -DSIV3D_BUILD_SAMPLE=ON` で configure し直してください (vcpkg の依存ライブラリもその VS で再ビルドされます)。

## 実行

```bat
cd SDL3\App\Release
Siv3DApp.exe
```

- ビルド後に `SDL3.dll` と、エンジンのリソース (`resources/engine/...`) とサンプル素材 (`example/`) が `SDL3/App/Release/` にコピーされます。
- リソースはカレントディレクトリではなく**実行ファイルの隣の `resources/`** から読み込まれます (`FileSystem::ModulePath()` は実行ファイルのフルパス)。
- リソースのコピーは実行ファイルがリンクされたときだけ行われます (POST_BUILD)。リソースだけを更新したいときは `Siv3DApp.exe` を消してからビルドし直してください。

## しくみ (Windows 固有の部分)

- **描画**: OpenGL ES 3.0 コンテキストを SDL3 で作成します (GPU ドライバの GLES 対応を使用)。
- **ファイルダイアログ**: SDL3 の `SDL_ShowFileDialogWithProperties` (コモンダイアログ)。Linux / macOS と共通の実装です (`SivDialog_SDL3.cpp`)。
- **ゴミ箱** (`FileSystem::Remove(path, AllowUndo::Yes)`): `SHFileOperationW` (`FOF_ALLOWUNDO`)。
- **音声**: SoLoud の miniaudio バックエンド (WASAPI) で再生します。起動ログに `Audio backend: MiniAudio` と出ます。オーディオデバイスが無い場合は NoSound バックエンドにフォールバックします。
- **音声ファイル**: WAVE / Ogg Vorbis / Opus / MIDI は共通実装、MP3 / FLAC は soloud 同梱の dr_mp3 / dr_flac でデコードします (全 OS 共通)。
- **動画** (`VideoReader` / `VideoTexture`): WebM (VP8 / VP9) は [movie-player](https://github.com/wamsoft/movie-player) (libvpx) で、それ以外 (MP4 など) は OpenCV の Media Foundation (MSMF) バックエンドで読み込みます。
  - `VideoTexture(path, Loop::Yes, PlayAudio::Yes)` で作成すると、音声トラック (Vorbis / Opus) を持つ WebM は映像と音声を同期して再生します (音声は `getAudio()` の `Audio` から鳴り、`pause()` すると映像も止まります)。
  - movie-player は CMake の FetchContent で取得し、Siv3D に組み込みます。`-DSIV3D_WITH_MOVIEPLAYER=OFF` で外せます (その場合 WebM は OpenCV に任せます)。
  - 色変換の libyuv は、movie-player 同梱のビルド済みライブラリ (clang でビルドした SIMD 有効版、静的 CRT) を使います。

## 既知の制限・注意点

- AAC (`.m4a`) / WMA は読み込めません (空の `Audio` / `Wave` になります)。SDL3 版の AudioCodec は MP3 / FLAC のみ対応です。
- `AudioFormat` へのエンコード (`Wave::save` で MP3 など) は WAVE / Ogg Vorbis / Opus だけです。
- ピッチシフト (`Audio::setPitchShift` 等の SoundTouch 連携) は無効です (`NullSoundTouchFunctions.cpp`)。
- 確認済みなのは x64 の Release ビルドだけです。

## 動作確認

2026-11-15、Windows 11 (x64) / Visual Studio 2026 で `Siv3DApp.exe` (Release) を起動し、次を確認しました。

- `SDL3/App/Main.cpp` のデモ (テクスチャ・絵文字・MSDF フォント・GUI) の描画
- ファイルダイアログ 4 種 (OpenFile / OpenFiles / SaveFile / SelectFolder) のタイトル・フィルタ・初期フォルダ・戻り値・キャンセル、ダイアログ表示中に終了要求を送ったときに空の結果で戻って正常終了すること
- ゴミ箱への移動
- `example/test.mp3` (MP3) と `example/midi/test.mid` (MIDI) の読み込みと再生、`example/video/river.mp4` を `VideoReader` で開いてフレームを取得できること
- WebM (VP9 1080p 30fps、音声付き、DefaultDuration 無し) を `VideoReader` で開き、フレームレートの推定 (30.000)・フレームの取得・シークができること、`VideoTexture` で描画できること
- 同じ WebM を `VideoTexture(path, Loop::Yes, PlayAudio::Yes)` で音声付き再生し、再生位置が音声の時計に従って進むこと、`getAudio().pause()` で映像も止まること、`setPosSec()` でシークできること
- 音声が Vorbis (2ch)・Opus (2ch)・Opus (1ch) の WebM で、トラック構成の判定 (ログ) と音声付き再生ができること。3 本とも実際に音が聞こえ、映像と音声がずれていないことを耳で確認
