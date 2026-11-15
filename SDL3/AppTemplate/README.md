# Siv3D (SDL3 版) アプリのテンプレート

OpenSiv3D (SDL3 版) を使うアプリのひな形です。このフォルダを丸ごとコピーして、自分のアプリのフォルダ (リポジトリ) にします。

アプリ側に置くのは **ソース・アセット・パッケージ定義 (`siv3d-app.json`)・最小限の `CMakeLists.txt` と `Makefile`** だけです。
エンジン本体 (OpenSiv3D) はコピーせず、`SIV3D_BASE` で参照します。ビルドの共通処理は OpenSiv3D 側の
`cmake/Siv3DApp*.cmake` (`siv3d_add_app()`) と `tools/siv3d_app.py` (ビルドドライバ) にあります。

## 構成

```
MyApp/
  CMakeLists.txt     OpenSiv3D を参照して siv3d_add_app() を呼ぶだけ
  siv3d-app.json     パッケージ定義 (表示名・実行ファイル名・バージョン・アセット・機種別の設定)
  Makefile           tools/siv3d_app.py の薄いラッパ
  src/               アプリのソース
  assets/            アプリのアセット (実行ファイルの隣に assets/ としてコピーされる)
  icon/              アイコン (Windows: .ico / macOS: .icns)
  build/<機種>/       出力 (git 管理外)
    cmake/           CMake のビルドツリー
    dist/            配布用の構成 (実行ファイル + SDL3 + resources/engine + アセット)
    package/         配布物 (<exeName>-<version>-<機種>.zip / .tar.gz)
```

## OpenSiv3D の場所 (`SIV3D_BASE`)

次の順に探します (CMake・Makefile とも同じ)。

1. 環境変数 `SIV3D_BASE` (OpenSiv3D リポジトリのルート)
2. このテンプレートが OpenSiv3D の中 (`SDL3/AppTemplate`) にある場合はそのルート
3. 隣にある `../OpenSiv3D`

```
work/
  OpenSiv3D/      ← SIV3D_BASE
  MyApp/          ← このテンプレートをコピーしたもの
```

## 必要なもの

- 各機種の OpenSiv3D のビルド環境 (`SDL3/BUILD-Windows.md` / `SDL3/BUILD_LINUX.md` / `SDL3/BUILD-macOS.md`)
- 環境変数 `VCPKG_ROOT`
- Python 3.9 以上と CMake 3.21 以上、Ninja
- Linux 版を Windows / macOS でビルドする場合: docker と sniper SDK のイメージ (`deckbuild-sniper`)

## ビルド

```bash
make                 # 実行中の OS 向けに build + stage + package
make windows         # Windows 版 (Windows で)
make macos           # macOS 版 (macOS で)
make linux           # Linux 版 (Windows / macOS ではコンテナ、Linux でもコンテナ)
make linux NATIVE=1  # Linux 版をホストで直接ビルド (Linux で)
make run             # stage した実行ファイルを起動
make build CONFIG=Debug
make clean TARGET=windows
```

`make` を使わずに直接呼ぶこともできます。

```bash
python <SIV3D_BASE>/tools/siv3d_app.py --project . --target windows all
```

CMake を直接使うこともできます (IDE で開く場合など)。

```bash
cmake -S . -B build/dev -G "Ninja Multi-Config"
cmake --build build/dev --config Release
```

| ホスト | 作れる機種 |
|---|---|
| Windows | windows、linux (docker のコンテナ) |
| Linux | linux |
| macOS | macos、linux (docker のコンテナ) |

Web / Android / iOS は今後対応予定です。

## パッケージ定義 (`siv3d-app.json`)

| キー | 意味 |
|---|---|
| `name` | 表示名 (ウィンドウのタイトルではなく、バージョン情報や .app のバンドル名) |
| `exeName` | 実行ファイル名 (`.exe` / `.app` の名前、配布物のファイル名にも使う) |
| `version` | バージョン (`1.2.3`)。Windows のバージョン情報、macOS の CFBundleVersion、配布物のファイル名 |
| `bundleId` | macOS のバンドル ID |
| `company` / `copyright` | Windows のバージョン情報、macOS の著作権表示 |
| `assets` | 実行ファイルの隣にコピーするフォルダ。`[{ "from": "assets", "to": "assets" }]` (`from` はこのフォルダからの相対パス) |
| `cmake.options` | CMake に渡すオプション (`SIV3D_WITH_MOVIEPLAYER` / `SIV3D_WITH_FFMPEG` など)。機種別の `<機種>.cmake.options` で上書きできる |
| `windows.console` | `true` でコンソールウィンドウを出す (既定 `false`) |
| `windows.icon` / `macos.icon` | アイコンファイル (`.ico` / `.icns`) |
| `linux.container` | Linux 版のビルドに使う docker イメージ (既定 `deckbuild-sniper`) |
| `<機種>.package` | 配布物の形式の配列: `zip` / `tar.gz` / `dir` |

アセットはカレントディレクトリ (実行ファイルのフォルダ。macOS は `.app/Contents/MacOS`) からの相対パスで読み込みます。

```cpp
const Texture texture{ U"assets/windmill.png" };
```

## 配布物の中身

- Windows: `MyApp.exe`、`SDL3.dll`、`resources/engine/`、アセット
- Linux: `MyApp` (RPATH = `$ORIGIN`)、`libSDL3.so.0*`、`resources/engine/`、アセット、
  (`SIV3D_WITH_FFMPEG=ON` のとき) FFmpeg の `.so` と `licenses/`
- macOS: `MyApp.app` (ANGLE の dylib を `Contents/Frameworks` に、リソースとアセットを `Contents/MacOS` に同梱)

コード署名 (Windows の Authenticode、macOS の codesign / 公証) はまだ行いません。
