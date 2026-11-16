# OpenSiv3D (SDL3 版) iOS ビルド手順

SDL3 + OpenGL ES 3.0 バックエンドを iOS でビルドし、シミュレータや実機で動かす手順です。

- 描画は SDL の UIKit バックエンドが作る OpenGL ES (EAGL) のコンテキストを使います (macOS と違って ANGLE は使いません)。
- SDL3 と vcpkg の依存ライブラリは、すべて静的リンクします (iOS のアプリは dylib を読み込めないため)。
- ビルドには CMake の Xcode ジェネレータを使います。`CMAKE_SYSTEM_NAME=iOS` で configure します。

## 必要なもの

| ツール | 確認済みバージョン |
|---|---|
| macOS | 15.8 (Intel x86_64) |
| Xcode | 26.3 (iOS SDK 26.2、iOS シミュレータのランタイム) |
| CMake | 3.31 (3.21 以上) |
| vcpkg | 環境変数 `VCPKG_ROOT` を設定しておく |
| Python | 3.9 以上 (`tools/siv3d_app.py` を使う場合) |

シミュレータは、ホストと同じ CPU 向けにビルドします (Intel Mac は x86_64、Apple Silicon は arm64)。実機は arm64 です。

## アプリをビルドする (siv3d_app のテンプレート)

アプリのテンプレート (`SDL3/AppTemplate` または siv3d_app リポジトリ) を使う場合は、ビルドドライバ `tools/siv3d_app.py` に `--target ios` を指定します。

```bash
# シミュレータ向けにビルド → stage (.app) → package (zip)
python3 <SIV3D_BASE>/tools/siv3d_app.py --project . --target ios all
# 起動中のシミュレータ (無ければ iPhone を 1 台起動) にインストールして起動
python3 <SIV3D_BASE>/tools/siv3d_app.py --project . --target ios run

# 実機 (arm64) 向け。package は ipa (Payload/<exe>.app)
python3 <SIV3D_BASE>/tools/siv3d_app.py --project . --target ios --sdk device all
```

テンプレートの Makefile では `make ios` / `make ios-run` / `make ios-device` です。

出力は `build/ios/<sdk>-<arch>/` の下にできます。

```
build/ios/simulator-x86_64/
  cmake/      CMake のビルドツリー (<プロジェクト名>.xcodeproj)
  dist/       <exeName>.app
  package/    <exeName>-<version>-ios-simulator.zip (device は .ipa)
```

`run` はビルドしたものを、起動中のシミュレータ (無ければ利用できる iPhone を 1 台起動) にインストールして起動します。ログを見たいときは `xcrun simctl launch --console-pty <デバイス> <バンドル ID>` で起動してください。

### siv3d-app.json の "ios" 節

```json
"ios": {
  "displayName": "My App",          // ホーム画面に出る名前 (省略時は name)
  "bundleId": "com.example.myapp",  // 省略時はトップレベルの bundleId
  "version": "1.0.0",               // CFBundleShortVersionString (省略時はトップレベルの version)
  "build": "1",                     // CFBundleVersion
  "deploymentTarget": "15.0",       // 対応する iOS の最低バージョン
  "orientations": "landscape",      // "landscape" / "portrait" / "all" または UIInterfaceOrientation* の配列
  "deviceFamily": "1,2",            // 1 = iPhone、2 = iPad
  "team": "",                       // 実機の署名に使う DEVELOPMENT_TEAM (Apple Developer の Team ID)
  "cmake": { "options": { "SIV3D_WITH_MOVIEPLAYER": "OFF" } }
}
```

- Info.plist は `cmake/ios/Info.plist.in` からこの値で生成します。起動画面は `UILaunchScreen` (空) です。
- シミュレータは ad-hoc 署名 (`-`) なので、Team ID は要りません。実機は "Apple Development" の自動署名で、`team` が必要です。

## OpenSiv3D 本体のサンプルをビルドする

```bash
cmake -S . -B build/ios-sim -G Xcode \
  -DCMAKE_SYSTEM_NAME=iOS -DCMAKE_OSX_SYSROOT=iphonesimulator \
  -DCMAKE_OSX_ARCHITECTURES=x86_64 -DCMAKE_OSX_DEPLOYMENT_TARGET=15.0 \
  -DSIV3D_BUILD_SAMPLE=ON
cmake --build build/ios-sim --config Release -- -sdk iphonesimulator -arch x86_64
# → build/ios-sim/SampleApp/Release/Siv3DApp.app
```

- vcpkg の triplet は `CMAKE_OSX_SYSROOT` と `CMAKE_OSX_ARCHITECTURES` から自動で選ばれます (`x64-ios-simulator` / `arm64-ios-simulator` / `arm64-ios`。いずれも `cmake/vcpkg-triplets` の overlay triplet で、静的ライブラリ、iOS 15.0)。
- 初回の configure では、vcpkg が依存ライブラリ (OpenCV・Boost・curl など約 130 パッケージ) をソースからビルドします。

## しくみ (iOS 固有の部分)

- **プラットフォームの判定**: iOS では `SIV3D_PLATFORM(MACOS)` と `SIV3D_PLATFORM(IOS)` の両方が 1 になります。`MACOS` は Apple 系 (macOS / iOS) 共通の処理、`IOS` は iOS で使えない機能を外す分岐に使います。`SIV3D_PLATFORM_NAME` は `"iOS"` です。
- **エントリポイント**: iOS では `UIApplicationMain` を SDL が持ちます。`Siv3DMain.cpp` が `<SDL3/SDL_main.h>` を include するので `main` は `SDL_main` になり、SDL がアプリの起動処理の後で (メインスレッドで) 呼び出します。
- **ファイルの場所**: `.app` は平らな構成で、実行ファイル・エンジンのリソース・アセットを直下に置きます。起動時にカレントディレクトリを `.app` の直下に移すので、`Resource()` も相対パス (`assets/...`) もそのまま使えます。
  - エンジンのリソースは、iOS だけ `resources/` ではなく **`siv3d_resources/engine`** に置きます。`.app` の直下に `Resources` (大文字小文字を区別しない) というフォルダがあると、旧形式のバンドルとみなされて Info.plist が読まれず、インストールが "Missing bundle ID" で失敗するためです。アプリのアセットにも、直下に `Resources` という名前のフォルダは使わないでください。
- **画面のフレームバッファ**: iOS の EAGL には既定のフレームバッファ (0) が無く、SDL がビュー用のフレームバッファを作ります。GLES3 のレンダラは画面に描くとき `GetDefaultFramebuffer()` (iOS では `SDL_PROP_WINDOW_UIKIT_OPENGL_FRAMEBUFFER_NUMBER`、それ以外は 0) をバインドします。
- **Xcode からの CMake の再実行**: Xcode ジェネレータは、CMakeLists.txt などが変わると xcodebuild の中 (ZERO_CHECK) で CMake を再実行します。その環境 (`SDKROOT` や `IPHONEOS_DEPLOYMENT_TARGET`) のまま vcpkg が動くと、ホスト用のツール (flatc など) が iOS 向けにビルドされて失敗し、バイナリキャッシュにも残ります。`cmake/Siv3DVcpkg.cmake` が xcodebuild の中ではこれらの環境変数を取り除き、`siv3d_app.py` は iOS のビルドの前に必ず xcodebuild の外で configure します。
- **SIMD**: arm64 (実機と Apple Silicon のシミュレータ) は simde 経路を使います。Intel Mac のシミュレータ (x86_64) は、macOS と同じく SSE4.1 の経路を使います (既定では SSE4.1 が無効なので `-msse4.1` を付けています)。
- **音声**: soloud の miniaudio バックエンド (Core Audio / AVAudioSession) を使います。miniaudio は AVFoundation を使うので、`soloud_miniaudio.cpp` は iOS では Objective-C++ としてコンパイルします。

## iOS で使えない・制限のある機能

| 機能 | iOS での動作 |
|---|---|
| ファイルダイアログ (`Dialog::OpenFile` など) | SDL3 が iOS に対応していないため、常に空の結果 |
| `DirectoryWatcher` | 未対応 (常に非アクティブ) |
| `System::LaunchBrowser` / `LaunchFile` | `SDL_OpenURL` で開く (ローカルのファイルは `file://`) |
| `System::ShowInFileManager` / `LaunchFileWithTextEditor` | false を返す |
| シリアル通信 | ポートの列挙は常に空 |
| WebM (movie-player) | 既定で OFF。vcpkg の libvpx が iOS シミュレータ向けにビルドできないため (`SIV3D_WITH_MOVIEPLAYER=ON` で実機向けは有効にできる見込み) |
| ディスプレイ情報 (infoware) | 空 |

## 確認状況

- iOS シミュレータ (x86_64、Intel Mac、iOS 26.3 / 18.5): テンプレートの MyApp が起動し、画像 (assets/windmill.png) と文字 (MSDF フォント) が横向きで表示され、音声 (MiniAudio) も初期化されることを確認。描画は Apple のソフトウェアレンダラ (OpenGL ES 3.0) です。
- 実機 (arm64-ios): ビルド (コンパイルとリンク、署名なし) まで確認。実機での起動は未確認 (Team ID による署名が必要)。

## うまくいかないとき

- **`simctl` が応答しない**: Mac の画面がロックされている間は、シミュレータの操作 (`simctl list` など) が固まることがあります。ロックを解除してから実行してください。固まったままなら `killall -9 com.apple.CoreSimulator.CoreSimulatorService` で再起動できます。
- **起動直後に AURemoteIO の RPC タイムアウトで落ちる**: シミュレータの音声サービスの状態によるもので、iOS 18.0 の一部のデバイスで起きました。別のデバイスやランタイムで起動するか、シミュレータの「デバイスを消去」を試してください。
