# OpenSiv3D (SDL3 版) アプリのライセンス表記

Siv3D のアプリを配布するときは、アプリに組み込まれたライブラリのライセンス文 (著作権表示と許諾条件) を、
利用者が見られるようにする必要があります。Siv3D はそのためにライセンス文をエンジンに内蔵していて、
`LicenseManager` で表示・取得できます。ここでは SDL3 版での扱いと、機種ごとに必要な対応をまとめます。

## 内蔵のライセンス一覧

`LicenseManager::EnumLicenses()` が返す一覧 (`LicenseManager::ShowInBrowser()` で HTML にして表示するもの) は、
次の 2 つを合わせたものです。

- 元の Siv3D の一覧 (`Siv3D/src/Siv3D/LicenseManager/LicenseList.hpp`、81 件): Siv3D Engine、FreeType、HarfBuzz、
  libpng / libjpeg-turbo / libwebp / libtiff / GIFLIB、libogg / libvorbis / opus、OpenCV、libcurl、zlib、Noto Emoji など
- SDL3 版で加わるもの (`LicenseList_SDL3.hpp`)。**機種・ビルドオプションで実際に組み込まれるものだけ** を登録します:

| 項目 | ライセンス | 入る機種・条件 |
|---|---|---|
| SDL | zlib | SDL3 版のすべて (Android の Java 側 `org.libsdl.app` も SDL の一部) |
| movie-player | MIT | WebM の再生 (`SIV3D_WITH_MOVIEPLAYER`。iOS と Web 以外の既定) |
| libvpx | BSD-3-Clause | 同上 |
| nestegg | ISC | 同上 |
| libyuv | BSD-3-Clause | 同上 |
| ANGLE | BSD-3-Clause | macOS (dylib を `.app` に同梱) |
| Emscripten | MIT | Web (wasm と一緒に出力される JS のランタイム) |

文面は各ライブラリの LICENSE (vcpkg の `share/<port>/copyright` など) をそのまま使っています。ライブラリを足したり
版を上げたりしたときは、この表と `LicenseList_SDL3.hpp` を見直してください。

アプリ自身のライセンスや、アプリが使う素材 (画像・音声・フォントなど) のライセンスは、アプリ側で追加します。

- **ファイルから追加** (SDL3 版で追加): `LicenseManager::AddLicenseFiles(U"assets/licenses")` で、フォルダ内の `*.txt` を
  ファイル名の順に読み込んで追加します。ファイルは UTF-8 のテキストで、先頭に `Title: 名前` と `Copyright: 著作権表示`
  (複数行は繰り返す) を書き、空行の後に許諾条件の本文を書きます (`Title:` が無ければファイル名を名前にする)。
  テンプレート siv3d_app は `assets/licenses/` にエンジン同梱のフォントのライセンスを置き、起動時にこれを呼んでいます。
- **コードから追加**: `LicenseManager::SetApplicationLicense()` / `LicenseManager::AddLicense()`。

一覧の `text` (許諾条件の本文) は普通のテキストとして扱い、`ShowInBrowser()` が HTML にするときにエスケープします
(SDL3 版での変更。以前は `SetApplicationLicense()` だけが登録時にエスケープし、`AddLicense()` の本文はそのまま HTML に
書き出していた)。`&` や `<` もそのまま書けます。

## 機種ごとの表示方法

| 機種 | 既定の表示 (F1 キー → `ShowInBrowser()`) | アプリ側で必要な対応 |
|---|---|---|
| Windows / macOS / Linux | 既定のブラウザでローカルの HTML を開く | 特になし (F1 キーを無効にする場合は、メニューなどから `ShowInBrowser()` を呼ぶ) |
| Web | 新しいタブで開く。ポップアップがブロックされたらページの上に重ねて表示する (閉じるボタン付き) | 特になし。ブラウザによっては F1 キーがヘルプに取られるので、ボタンなどから `ShowInBrowser()` を呼ぶとよい |
| Android | **表示できない** (アプリ内の HTML を他のアプリに渡せない。そもそも F1 キーが無い) | **ライセンス画面をアプリ内に作る**: `EnumLicenses()` の `title` / `copyright` / `text` を表示する |
| iOS | **表示できない** (同上) | 同上 |

Android と iOS では、設定画面などに「ライセンス」の項目を置き、`EnumLicenses()` の内容をスクロールして読める形で
表示してください (ストアの審査で求められることもあります)。
テンプレート [siv3d_app](https://github.com/wtnbgo/siv3d_app) の `src/LicenseView.hpp` が、その例です (折り返し・スクロール・閉じる。
右下の [Licenses] ボタンから開き、Android / iOS 以外ではブラウザで開く)。

```cpp
// 例: 一覧を順に表示する (実際にはスクロールや改行の処理を加える)
for (const auto& license : LicenseManager::EnumLicenses())
{
	// license.title (名前)、license.copyright (著作権表示)、license.text (許諾条件) を表示する
}
```

## 配布物に同梱するもの

- **Linux**: `SIV3D_WITH_FFMPEG=ON` (既定) のときは FFmpeg (LGPL 2.1) を共有ライブラリで同梱します。配布物の
  `licenses/ffmpeg.txt` (ライセンス文) と `licenses/ffmpeg-source.txt` (ソースの入手先) を **必ず一緒に配布** してください。
  共有ライブラリなので差し替えて動かすことができ、LGPL の再リンクの要件も満たします (`SDL3/BUILD_LINUX.md`)。
  FFmpeg 以外に LGPL / GPL のライブラリは入りません。
- **その他の機種**: 追加で同梱するライセンスファイルはありません (ライセンス文はエンジンに内蔵)。

## 一覧に入れていないもの

次のものは、配布物での表示の義務が無い (または元の Siv3D が一覧に入れていない) ため、一覧に入れていません。

- **C++ の標準ライブラリ**: libc++ (Android の `libc++_shared.so`、Web、macOS / iOS) は Apache License 2.0 + LLVM 例外で、
  バイナリの配布では表示の義務がありません。MSVC / libstdc++ のランタイムも同様です。
- **Boost** (Boost Software License): バイナリの配布では表示の義務がありません。
- 元の Siv3D の一覧に無い組み込みのライブラリ (SoLoud / miniaudio / stb など): 元の Siv3D の方針に合わせています。
  厳密に扱う場合は `LicenseManager::AddLicense()` で追加してください。

## エンジンのリソース (フォントなど)

`resources/engine/` のフォント (Noto Sans CJK、M+、Font Awesome、Material Design Icons、Noto Emoji) は、元の Siv3D と
同じものです。元の Siv3D の一覧には Noto Emoji と Material Design の項目はありますが、**Noto Sans CJK・M+・Font Awesome の
項目はありません**。Noto Sans CJK と Font Awesome のフォントは SIL Open Font License (OFL) で、フォントを同梱するときに
著作権表示とライセンス文を添えることを求めています。テンプレート siv3d_app は、この 3 つのライセンスを `assets/licenses/`
(`noto-sans-cjk.txt` / `mplus.txt` / `font-awesome.txt`) に置き、`AddLicenseFiles()` で一覧に追加しています
(使わないフォントを `resources/engine/font/` から外した場合は、対応するファイルも消してかまいません)。
予備のフォント `min/siv3d-min.woff` は、Noto Sans JP 2.004 (© 2014-2021 Adobe、OFL) から英数字・記号など 198 文字を
抜き出したもの (フォントの name テーブルで確認) なので、`noto-sans-cjk.txt` のライセンスに含まれます。
