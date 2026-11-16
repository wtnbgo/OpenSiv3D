# OpenSiv3D (SDL3 版) Web ビルド手順

SDL3 + OpenGL ES 3.0 (WebGL 2) バックエンドを Emscripten で WebAssembly にビルドし、ブラウザで動かす手順です。
デスクトップ版と同じ SDL3 版のソースをそのまま使います (以前の Web 版 `Web/CMakeLists.txt` とは別物です)。

## 必要なもの

| ツール | 確認済みバージョン |
|---|---|
| emsdk | 6.0.6 (`emsdk install latest`。環境変数 `EMSDK` に emsdk のフォルダ) |
| CMake / Ninja | Windows は Visual Studio 同梱版 (開発者環境から使う) |
| vcpkg | 環境変数 `VCPKG_ROOT` |
| Python | 3.9 以上 (確認用サーバ) |

どのホスト (Windows / Linux / macOS) でもビルドできます。

## ビルド

```bash
# 本体とサンプル (SDL3/App/Main.cpp)
cmake --preset wasm32-emscripten -DSIV3D_BUILD_SAMPLE=ON
cmake --build build/wasm32-emscripten --config Release

# 確認 (COOP/COEP ヘッダ付きで配信する)
python SDL3/Web/serve.py 8080 SDL3/App/Release
#  → http://127.0.0.1:8080/Siv3DApp.html
```

アプリ (テンプレート [siv3d_app](https://github.com/wtnbgo/siv3d_app)) からは:

```bash
python <SIV3D_BASE>/tools/siv3d_app.py --target web all   # build/web/dist と build/web/package/<exeName>-<version>-web.zip
python <SIV3D_BASE>/tools/siv3d_app.py --target web run   # serve.py で配信
```

- 初回の configure で vcpkg が依存ライブラリを wasm 向けにビルドするので時間がかかります。
- 出力は `<exeName>.html` / `.js` / `.wasm` / `.data` と `serve.py`。`.data` にエンジンのリソースとアセットが入ります。

## 配信の条件

Web 版はスレッド (SharedArrayBuffer) を使うため、**`Cross-Origin-Opener-Policy: same-origin` と
`Cross-Origin-Embedder-Policy: require-corp` のヘッダを付けて配信する必要があります**。`file://` や、ヘッダを付けない
静的ホスティングでは動きません (同梱の `serve.py` は付けています)。

## しくみ

- **ビルド設定**: `cmake/toolchains/wasm32-emscripten.cmake` (Emscripten.cmake に `-pthread -fwasm-exceptions` を足す) を
  vcpkg の overlay triplet `wasm32-emscripten` から chainload し、依存ライブラリと本体で ABI をそろえます。
- **メインループ**: Siv3D のアプリは `while (System::Update())` で処理を止める書き方なので、`CRenderer_GLES3::present` で
  `SDL_GL_SwapWindow` の後に `requestAnimationFrame` まで待ち (EM_ASYNC_JS + ASYNCIFY)、ブラウザに制御を返します。
- **描画**: WebGL 2。GLES3 のシェーダ (`shader/essl`) はそのまま使えます。
- **ファイル**: エンジンのリソースは `/resources/engine/`、アセットは `/<to>/` に置かれ、カレントディレクトリは `/` です。
- **セーブデータ**: ホームフォルダ `/home/<exeName>` をブラウザの IndexedDB に保存される IDBFS にしています
  (`SDL3/Web/persist.pre.js.in` を `--pre-js` で組み込む)。起動時 (`Main` の前) に IndexedDB の内容を読み込み、
  ファイルを閉じたとき・削除や名前の変更のときに自動で書き戻します (IDBFS の autoPersist)。
  `SpecialFolder::Documents` / `LocalAppData` / `UserProfile` などはホームフォルダの下を指すので、そこに書けば保存されます。
  エンジンのキャッシュ (展開したフォントなど) と `FileSystem::TemporaryDirectoryPath()` は `/tmp/` に置き、保存しません。
  IndexedDB はサイト (オリジン) ごとで、データベース名がホームフォルダのパスなので、同じサイトに置いた別のアプリとは
  `exeName` で分かれます。`siv3d-app.json` の `web.persist` を `false` にすると無効になります。
- **HTTP**: curl の代わりに Emscripten の fetch API (`Siv3D/src/Siv3D/Network/WebFetch.hpp`)。ブラウザの制約 (CORS) を受けます。
- **音声**: SoLoud の miniaudio (Web Audio)。
- **依存ライブラリ**: vcpkg の harfbuzz ポートは emcc の `-Werror` で失敗するので、ソースを FetchContent してまとめ済みソースを
  直接ビルドします。opencv4 は最小構成 (calib3d / jpeg / png / webp / quirc、V4L 無効)。curl は使いません。

## 既知の制限

- **スクリプト (AngelScript)**: 無効です (wasm からネイティブ関数を呼べないため。以前の Web 版と同じ)。
- **動画**: movie-player (libvpx) は vcpkg の libvpx が wasm に対応していないため無効です。OpenCV の動画も読めません。
- **ダイアログ**: SDL3 のファイルダイアログは Web では使えず、結果なしが返ります。
- **ファイルの変更監視・ゴミ箱**: 無効です。
- **セーブデータ**: カレントディレクトリ (`/`) の直下に書いたファイルは保存されません (ページを閉じると消えます)。ホームフォルダの下に書いてください。
  ブラウザのプライベートモードや、サイトのデータを消したときは消えます。
- **音声の自動再生**: ブラウザの自動再生制限により、ページを操作するまで音が出ないことがあります。
- 一瞬で押して離す操作 (自動化ツールのクリックなど) は取りこぼすことがあります (人の操作では問題ありません)。
- ページの読み込みは `.data` (エンジンのリソース約 38MB + アセット) のダウンロードを含みます。

## 動作確認

2026-11-16、Windows 11 / emsdk 6.0.6 でビルドし、Chromium 系ブラウザで `SDL3/App/Main.cpp` のデモ (テクスチャ・絵文字・
MSDF フォント・日本語・GUI) の描画とマウス入力 (GUI のボタン) を確認しました。テンプレート (siv3d_app) の `--target web` で
zip まで作れることも確認しました。
