# OpenSiv3D (SDL3 版) アプリの署名と配布の手順

[siv3d_app](https://github.com/wtnbgo/siv3d_app) のテンプレートとドライバ `tools/siv3d_app.py` でビルドしたアプリを、
配布用に署名するための手順です。証明書・鍵は開発者や組織ごとに用意するものなので、ここでは **ビルドの流れのどこで・
何を実行するか** をまとめます (自動化はしていません。現状で自動で行うのは macOS の ad-hoc 署名と、iOS 実機用の開発署名だけです)。

## 共通: 署名を入れる場所

ドライバは `build` → `stage` (配布物の構成を `build/<機種>/dist/` に作る) → `package` (zip / tar.gz / apk などに固める) の順に動きます。
署名は **`stage` の後、`package` の前** に、`dist/` の中身に対して行います。

```bash
make build TARGET=windows      # (macOS / Linux も同様)
make stage TARGET=windows      # build/windows/dist/ ができる
# ← ここで dist/ の中身に署名する
make package TARGET=windows    # 署名済みの dist/ を固める
```

- 証明書・鍵・パスワードは **リポジトリに入れない**。証明書の名前や Team ID のような値は `siv3d-app.local.json`
  (git の管理外。`siv3d-app.json` に重ねて読まれる) に、パスワードは環境変数や OS のキーチェーン・CI のシークレットに置く。
- 署名した後に `stage` をやり直すと `dist/` が作り直されて署名が消えるので、署名はもう一度行う。

| 機種 | 現状 | 配布に必要なもの |
|---|---|---|
| Windows | 署名なし | コード署名証明書 (OV / EV、またはクラウド署名) と signtool |
| macOS | ad-hoc 署名 (`macos.sign`、既定 `-`) | Developer ID Application 証明書、公証 (notarytool) |
| iOS | 実機用は開発署名 (Xcode の自動署名、`ios.team`) | 配布用は Apple Distribution 証明書と App Store Connect (TestFlight / App Store) |
| Android | release も debug の鍵で署名 | 配布用の鍵 (keystore)。Google Play は AAB と Play App Signing |
| Linux | なし (一般に不要) | Steam で配る場合は Steamworks の depot。必要ならハッシュや GPG 署名を添える |
| Web | なし | HTTPS で配信 (COOP / COEP ヘッダが必要。`SDL3/BUILD-Web.md`) |

## Windows

署名するのは `dist/` の **.exe と .dll すべて** (`<exeName>.exe`、`SDL3.dll` など)。signtool は Windows SDK に入っています
(開発者コマンドプロンプトなら PATH が通っている)。

```bat
make build TARGET=windows
make stage TARGET=windows

rem 証明書ストアの証明書で署名する例 (/sha1 は証明書の拇印。/tr は認証局が案内するタイムスタンプ サーバー)
signtool sign /fd SHA256 /td SHA256 /tr http://timestamp.digicert.com /sha1 <拇印> build\windows\dist\*.exe build\windows\dist\*.dll

rem 確認
signtool verify /pa /v build\windows\dist\MyApp.exe

make package TARGET=windows
```

- PFX ファイルを使う場合は `/sha1` の代わりに `/f <file.pfx> /p <パスワード>` (パスワードは環境変数から渡し、履歴に残さない)。
- EV 証明書 (ハードウェアトークン) やクラウド署名 (Azure Trusted Signing など) は、提供元の手順で signtool の
  `/dlib` などを指定する。どれも対象ファイルと順番 (stage の後・package の前) は同じ。
- 署名しないと、ダウンロードした exe の実行時に SmartScreen の警告が出る。EV 証明書か、評判が蓄積されるまで警告は消えない。

## macOS

`stage` で `.app` を ad-hoc 署名しています (`siv3d-app.json` の `macos.sign`。既定は `"-"`、`false` で署名しない)。
配布用は **Developer ID Application 証明書で署名 → 公証 (notarization) → ステープル** が必要です。
公証には Hardened Runtime (`--options runtime`) とタイムスタンプが要るので、ドライバの署名は止めて手で署名します。

1. `siv3d-app.local.json` で ad-hoc 署名を止める:
   ```json
   { "macos": { "sign": false } }
   ```
2. ビルドして、`.app` に署名する (中の dylib・フレームワークも含めて、内側から順に署名される):
   ```bash
   make build TARGET=macos
   make stage TARGET=macos
   APP=build/macos/dist/MyApp.app
   codesign --force --deep --options runtime --timestamp \
     --sign "Developer ID Application: <名前> (<TEAMID>)" "$APP"
   codesign --verify --strict --verbose=2 "$APP"
   ```
3. 公証に出して、結果を `.app` にステープルする (`notarytool store-credentials` で認証情報をキーチェーンに保存しておく):
   ```bash
   ditto -c -k --keepParent "$APP" /tmp/MyApp-notarize.zip
   xcrun notarytool submit /tmp/MyApp-notarize.zip --keychain-profile <プロファイル名> --wait
   xcrun stapler staple "$APP"
   spctl --assess --type execute --verbose "$APP"   # "accepted source=Notarized Developer ID" になること
   ```
4. 固める: `make package TARGET=macos`

- 公証が拒否されたら `xcrun notarytool log <submission-id> --keychain-profile <プロファイル名>` で理由を確認する
  (署名されていない dylib、Hardened Runtime 無し、タイムスタンプ無しなど)。
- Hardened Runtime で動かない機能が見つかった場合は、エンタイトルメント (`--entitlements <file>`) で必要な例外だけを許可する。
- `--deep` はネストした .app などがあると不十分なことがある。その場合は中の dylib を先に個別に署名してから `.app` を署名する。

## iOS

実機用のビルド (`make ios-device`) は、`ios.team` (Team ID。`siv3d-app.local.json` に書く) と Xcode の自動署名で
**開発用 (Apple Development)** に署名され、`.ipa` は登録済みの実機にしか入りません。TestFlight / App Store に出すには、
Apple Distribution の証明書でアーカイブを書き出し、App Store Connect にアップロードします。

1. 実機用にビルドして、Xcode のプロジェクトを作る (Team ID は `siv3d-app.local.json` の `ios.team`):
   ```bash
   make ios-device
   ```
2. アーカイブして、App Store Connect 向けに書き出す (`build/ios/device-arm64/cmake/` にある Xcode のプロジェクトを使う。名前はアプリの CMakeLists.txt の `project()`):
   ```bash
   xcodebuild -project build/ios/device-arm64/cmake/MyApp.xcodeproj \
     -scheme MyApp -configuration Release -sdk iphoneos \
     -archivePath build/ios/MyApp.xcarchive -allowProvisioningUpdates archive
   xcodebuild -exportArchive -archivePath build/ios/MyApp.xcarchive \
     -exportOptionsPlist ExportOptions.plist -exportPath build/ios/export -allowProvisioningUpdates
   ```
   `ExportOptions.plist` の例:
   ```xml
   <?xml version="1.0" encoding="UTF-8"?>
   <!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
   <plist version="1.0">
   <dict>
     <key>method</key><string>app-store-connect</string>
     <key>teamID</key><string>ABCDE12345</string>
     <key>signingStyle</key><string>automatic</string>
   </dict>
   </plist>
   ```
3. 書き出した `.ipa` を Transporter (または `xcrun altool --upload-app`) で App Store Connect にアップロードする。

- App Store Connect にアプリを登録するときのバンドル ID は、`siv3d-app.json` の `bundleId` (iOS だけ変える場合は `ios.bundleId`)。
- 版の番号は `version` (CFBundleShortVersionString) と `ios.build` (CFBundleVersion)。アップロードのたびに `ios.build` を増やす。
- 社内配布 (Ad Hoc / Enterprise) は `method` を `release-testing` / `enterprise` にする。

## Android

外枠 [siv3d_and](https://github.com/wtnbgo/siv3d_and) の `release` は、今は debug の鍵で署名しています
(`app/build.gradle` の `buildTypes.release.signingConfig signingConfigs.debug`)。配布用には自分の鍵で署名します。

1. 鍵を作る (一度だけ。**鍵を失くすと同じアプリの更新を出せなくなる**ので、安全な場所に保管する):
   ```bash
   keytool -genkeypair -v -keystore release.jks -alias release \
     -keyalg RSA -keysize 4096 -validity 10000
   ```
2. `siv3d_and/app/build.gradle` に release 用の `signingConfigs` を足し、値は Gradle のプロパティ
   (ユーザの `~/.gradle/gradle.properties` か `-P`。リポジトリの外) か環境変数から読む:
   ```groovy
   android {
       signingConfigs {
           release {
               storeFile     file(findProperty('siv3d.storeFile') ?: System.getenv('SIV3D_STORE_FILE') ?: 'release.jks')
               storePassword (findProperty('siv3d.storePassword') ?: System.getenv('SIV3D_STORE_PASSWORD'))
               keyAlias      (findProperty('siv3d.keyAlias') ?: System.getenv('SIV3D_KEY_ALIAS') ?: 'release')
               keyPassword   (findProperty('siv3d.keyPassword') ?: System.getenv('SIV3D_KEY_PASSWORD'))
           }
       }
       buildTypes {
           release {
               signingConfig signingConfigs.release
           }
       }
   }
   ```
3. ビルドして確認する:
   ```bash
   # ~/.gradle/gradle.properties に siv3d.storeFile=/path/to/release.jks などを書くか、環境変数 SIV3D_STORE_* を設定しておく
   python ../OpenSiv3D/tools/siv3d_app.py --target android all     # APK (siv3d-app.json の android.package)
   apksigner verify --print-certs build/android/dist/MyApp.apk     # 証明書が自分の鍵になっていること
   ```

- Google Play に出すときは AAB (`android.package` に `"aab"`) を作り、Play Console で **Play App Signing** を使う
  (アップロード鍵で署名した AAB を出し、配布用の署名は Google が行う)。上の鍵はアップロード鍵として使える。
- `versionCode` (`android.versionCode`) はアップロードのたびに増やす。

## Linux / Web

- Linux: 実行ファイルへのコード署名は一般に行いません。Steam で配る場合は Steamworks の depot にアップロードします
  (`SDL3/BUILD_LINUX.md` の配布条件を満たすコンテナ版を使う)。直接配る場合は、tar.gz に SHA-256 のハッシュや GPG 署名を添えます。
- Web: 署名はありません。HTTPS で配信し、COOP / COEP のヘッダを付けます (`SDL3/BUILD-Web.md`)。
