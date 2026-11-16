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
| 実機 | iPhone 11 (iOS 26.5) |
| CMake | 3.31 (3.21 以上) |
| vcpkg | 環境変数 `VCPKG_ROOT` を設定しておく |
| Python | 3.9 以上 (`tools/siv3d_app.py` を使う場合) |

シミュレータは、ホストと同じ CPU 向けにビルドします (Intel Mac は x86_64、Apple Silicon は arm64)。実機は arm64 です。

## アプリをビルドする (siv3d_app のテンプレート)

アプリのテンプレート ([siv3d_app](https://github.com/wtnbgo/siv3d_app) リポジトリ) を使う場合は、ビルドドライバ `tools/siv3d_app.py` に `--target ios` を指定します。

```bash
# シミュレータ向けにビルド → stage (.app) → package (zip)
python3 <SIV3D_BASE>/tools/siv3d_app.py --project . --target ios all
# 起動中のシミュレータ (無ければ iPhone を 1 台起動) にインストールして起動
python3 <SIV3D_BASE>/tools/siv3d_app.py --project . --target ios run

# 実機 (arm64) 向け。package は ipa (Payload/<exe>.app)
python3 <SIV3D_BASE>/tools/siv3d_app.py --project . --target ios --sdk device all
```

テンプレートの Makefile では `make ios` / `make ios-run` (シミュレータ) と `make ios-device` / `make ios-device-run` (実機) です。

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
  "team": ""                        // 実機の署名に使う DEVELOPMENT_TEAM (Apple Developer の Team ID。普通は siv3d-app.local.json に書く)
}
```

- `team` などの開発者ごとの値は、アプリのフォルダの `siv3d-app.local.json` (git の管理外にする) に書くと `siv3d-app.json` に重ねて読まれます。環境変数 `SIV3D_IOS_TEAM` / `SIV3D_IOS_DEVICE` でも上書きできます。`siv3d_app.py` は Team ID とバンドル ID を `-DSIV3D_IOS_TEAM` / `-DSIV3D_IOS_BUNDLE_ID_OVERRIDE` で CMake に渡します。
- 実機は `--sdk device` でビルドし (`-allowProvisioningUpdates -allowProvisioningDeviceRegistration` で自動署名・端末登録)、`run --sdk device [--device <UDID または名前>]` で `xcrun devicectl` を使ってインストールと起動を行います。手順はテンプレート (siv3d_app) の README の「iOS 実機で動かす」を参照してください。
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
- **終了要求**: iOS では、SDL が送る終了要求 (`SDL_EVENT_QUIT` / ウィンドウを閉じる要求。デバッガやコンソールの切断などのシグナルで届く) を受け付けません。iOS は `Main()` を抜けてもプロセスを終了させず、黒い画面のまま残ってしまうためです。アプリの終了は OS (ユーザのスワイプなど) に任せます。アプリが自分で `System::Exit()` を呼んだ場合は、同じく黒い画面で止まるので、iOS では呼ばないでください。
- **バックグラウンド**: ホーム画面に戻る・ほかのアプリに切り替えるなどでバックグラウンドに入り、戻ってきた後も描画は続きます。

## iOS で使えない・制限のある機能

| 機能 | iOS での動作 |
|---|---|
| ファイルダイアログ (`Dialog::OpenFile` など) | SDL3 が iOS に対応していないため、常に空の結果 |
| `DirectoryWatcher` | 未対応 (常に非アクティブ) |
| `System::LaunchBrowser` / `LaunchFile` | `SDL_OpenURL` で開く (ローカルのファイルは `file://`) |
| `System::ShowInFileManager` / `LaunchFileWithTextEditor` | false を返す |
| シリアル通信 | ポートの列挙は常に空 |
| WebM (movie-player) | 使えない (`SIV3D_WITH_MOVIEPLAYER` は iOS では常に OFF。vcpkg の libvpx が iOS シミュレータ向けにビルドできないため) |
| 文字入力 (ソフトウェアキーボード) | 入力欄が入力中の間だけキーボードを出す。SimpleGUI の `TextBox` / `TextArea` は自動。独自の入力欄は、入力中の間 `TextInput::SetEditingArea` を毎フレーム呼ぶ。入力欄の外をタップすると閉じ、ユーザが閉じた後はもう一度タップで出し直す。**シミュレータでは、ソフトウェアキーボードの ⌫ が効かない** (Mac のキーボードが接続されているとみなされ、SDL が `SDL_HasKeyboard()` のときにソフトウェアキーボードの BS を捨てるため)。実機での確認と SDL の修正は今後の検討事項 |
| ライセンス一覧 (`LicenseManager::ShowInBrowser` / F1) | ブラウザでは表示できない。アプリ内に表示する (テンプレートの `LicenseView.hpp` と [Licenses] ボタン) |
| ディスプレイ情報 (infoware) | 空 |
| CPU 情報 (`GetCPUInfo`、起動時のログ) | CPU 名・個数は取れない (ログは `Apple 0.0.0 (… 0)` となる。動作には影響しない) |

## 確認状況

- iOS シミュレータ (x86_64、Intel Mac、iOS 26.3 / 18.5): テンプレートの MyApp が起動し、画像 (assets/windmill.png) と文字 (MSDF フォント) が横向きで表示され、音声 (MiniAudio) も初期化されることを確認。描画は Apple のソフトウェアレンダラ (OpenGL ES 3.0) です。バックグラウンドから戻った後の描画、アプリ内のライセンス一覧 (表示・ドラッグでのスクロール・Close)、ソフトウェアキーボードの開閉と文字入力 (上の制限を除く) も確認済み。
- 実機 (iPhone 11、iOS 26.5): `siv3d-app.local.json` に Team ID を書いて `make ios-device` で署名付きでビルドし (Apple Development の自動署名)、`make ios-device-run` でインストールして起動。画像と文字が表示されることを確認。描画は Apple A13 GPU (OpenGL ES 3.0 Metal)、音声は MiniAudio、起動時の OpenGL のエラーなし。ソフトウェアキーボードでの文字入力は、実機では未確認です。

## うまくいかないとき

- **`simctl` が応答しない**: Mac の画面がロックされている間は、シミュレータの操作 (`simctl list` など) が固まることがあります。ロックを解除してから実行してください。固まったままなら `killall -9 com.apple.CoreSimulator.CoreSimulatorService` で再起動できます。
- **起動直後に AURemoteIO の RPC タイムアウトで落ちる**: シミュレータの音声サービスの状態によるもので、iOS 18.0 の一部のデバイスで起きました。別のデバイスやランタイムで起動するか、シミュレータの「デバイスを消去」を試してください。
