# OpenSiv3D (SDL3 版) macOS ビルド手順

SDL3 + OpenGL ES 3.0 バックエンドを macOS でビルドし、サンプルアプリ (`Siv3DApp.app`) を動かす手順です。

macOS には OpenGL ES が無いため、[ANGLE](https://chromium.googlesource.com/angle/angle) (Metal バックエンド) を vcpkg でビルドし、アプリバンドルに同梱して使います。

## 必要なもの

| ツール | 確認済みバージョン |
|---|---|
| macOS | 15.8 (Intel x86_64) |
| Xcode (Command Line Tools) | Apple clang 17 |
| CMake | 3.31 (3.20 以上) |
| Ninja | Homebrew 版 |
| vcpkg | 環境変数 `VCPKG_ROOT` を設定しておく |

## ビルド

```bash
# Intel Mac
cmake --preset x64-osx -DSIV3D_BUILD_SAMPLE=ON
cmake --build build/x64-osx --config Release

# Apple Silicon (未確認)
cmake --preset arm64-osx -DSIV3D_BUILD_SAMPLE=ON
cmake --build build/arm64-osx --config Release
```

- 初回の configure では vcpkg が依存ライブラリ (ANGLE・OpenCV・Boost など) をソースからビルドするため、時間がかかります (Intel Mac で約 45 分。ANGLE が大半)。
- SDL3 は CMake の FetchContent で取得されます。
- `SIV3D_BUILD_SAMPLE=ON` を付けると `SDL3/App/Main.cpp` からサンプルアプリ `Siv3DApp` をビルドします。

## 実行

```bash
open SDL3/App/Release/Siv3DApp.app
# または (ログがターミナルに出る)
SDL3/App/Release/Siv3DApp.app/Contents/MacOS/Siv3DApp
```

生成されるバンドルの構成:

```
Siv3DApp.app/Contents/
├── Info.plist
├── MacOS/
│   ├── Siv3DApp
│   ├── libSDL3.0.dylib
│   ├── resources/engine/   … エンジンリソース (essl シェーダ / フォント / サウンドフォント / テクスチャ)
│   └── example/            … サンプル用素材
└── Frameworks/
    ├── liblibEGL_angle.dylib
    └── liblibGLESv2_angle.dylib
```

- `resources/engine` は `Web/App/resources/engine` から、`example` は `WindowsDesktop/App/example` からビルド後に (POST_BUILD で) コピーされます (全プラットフォーム共通の処理)。
- `Resource()` は「実行ファイルのディレクトリ + `resources/`」を参照します。
- `.app` として起動した場合 (Finder や `open` から起動するとカレントディレクトリは `/`)、エンジンはカレントディレクトリを `Contents/MacOS/` に変更するため、`example/...` などの相対パスはそこから解決されます。コマンドライン引数でアプリディレクトリを渡した場合は、そちらが優先されます (`<dir>` に移動し、`<dir>/resources` をリソースディレクトリとして使います)。

## しくみ (macOS 固有の部分)

### ANGLE (OpenGL ES → Metal)

- `vcpkg.json` に `angle[metal]` (`"platform": "osx"`) を追加しています。
- SDL3 は GLES コンテキストを要求されたときに `libEGL` / `libGLESv2` を実行時に `dlopen` するため、ANGLE は静的ライブラリでは使えません。そこで overlay triplet (`cmake/vcpkg-triplets/{x64,arm64}-osx.cmake`) を使い、`angle` ポートだけを dylib としてビルドします (他のポートは静的のまま)。`osx` プリセットが `VCPKG_OVERLAY_TRIPLETS` でこのディレクトリを指定しています。
- `Siv3DApp` の POST_BUILD で ANGLE の dylib を `Contents/Frameworks` にコピーし、RPATH に `@executable_path/../Frameworks` を追加しています。
- エンジン (`CWindow::init`) は `SDL_Init` の前に、`SDL_HINT_OPENGL_LIBRARY` / `SDL_HINT_EGL_LIBRARY` で同梱 dylib の絶対パスを SDL に渡します。ファイル名は CMake が `SIV3D_ANGLE_GLES_LIB` / `SIV3D_ANGLE_EGL_LIB` として定義します。dylib は `Contents/Frameworks/` → 実行ファイルと同じディレクトリの順に探します。

### Retina (HiDPI)

- ウィンドウを `SDL_WINDOW_HIGH_PIXEL_DENSITY` で作成するため、フレームバッファは物理ピクセル解像度になります (例: 800x600 のウィンドウで 1600x1200)。`Window::GetState().scaling` は 2.0 になります。
- 既定のリサイズモード (`ResizeMode::Virtual`) ではシーンは 800x600 で描画され、拡大して表示されます (従来の macOS 版と同じ挙動)。高解像度で描画したい場合は `Scene::SetResizeMode(ResizeMode::Actual)` を使ってください。
- マウス座標は、SDL のポイント単位からフレームバッファのピクセル単位に変換しています。

### その他

- `DirectoryWatcher` は FSEvents による実装です (旧 `Siv3D-Platform/macOS` の実装を移植)。
- ファイルダイアログは SDL3 (`SDL_ShowFileDialogWithProperties`、NSOpenPanel / NSSavePanel) を使います。
- ゴミ箱 (`FileSystem::Remove(path, AllowUndo::Yes)`) は `NSFileManager trashItemAtURL` による実装 (`SivFileSystem_SDL3_macOS.mm`) です。このため CMake で `OBJCXX` 言語を有効にしています。Cocoa / CoreServices / IOKit などのフレームワークをリンクしています。

## 既知の制限・注意点

- 確認済みなのは Intel Mac (x86_64) の Release ビルドだけです。`arm64-osx` プリセットは用意してありますが、ビルドは未確認です (`Platform.hpp` は macOS を常に SSE ありとして扱うため、arm64 では対応が必要になる見込みです)。
- 起動時に `OpenGL Error: 0x500` (GL_INVALID_ENUM) が 1 回だけログに出ます。描画には影響ありません (原因は未調査)。
- 音声は `WITH_NOSOUND` (NoSound バックエンド) です (他のプラットフォームと同じ)。
- コード署名・公証は行っていません。配布するには別途 `codesign` が必要です。
