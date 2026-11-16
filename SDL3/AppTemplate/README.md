# アプリのテンプレートは siv3d_app リポジトリに移りました

Siv3D (SDL3 版) のアプリのテンプレートは、別リポジトリ **[wtnbgo/siv3d_app](https://github.com/wtnbgo/siv3d_app)** にあります。
以前ここにあったテンプレートと同じ構成 (`src/` / `assets/` / `icon/` / `siv3d-app.json` / `CMakeLists.txt` / `Makefile`) に、
次の機能が加わっています。

- 対応機種: Windows / Linux (Steam Runtime sniper のコンテナ) / macOS / iOS / Web / Android
- OpenSiv3D (`SIV3D_BASE`) が見つからないときの自動取得 (`siv3d-app.json` の `siv3d` 節で取得元と版を指定)
- 開発者ごとの設定 (`siv3d-app.local.json`。iOS 実機の Team ID など)

## 使い方

```bash
git clone https://github.com/wtnbgo/siv3d_app.git MyApp
cd MyApp
make            # 実行中の OS 向けに build + stage + package (隣に OpenSiv3D が無ければ取得する)
make web        # Web 版
make android    # Android 版 (外枠 wtnbgo/siv3d_and を隣に置く)
```

ビルドの仕組み (`cmake/Siv3DAppPrelude.cmake` / `cmake/Siv3DApp.cmake` / `cmake/Siv3DAppTarget.cmake` の `siv3d_add_app()`、
ドライバ `tools/siv3d_app.py`) はこの OpenSiv3D リポジトリにあり、テンプレートはそれを参照します。
機種ごとの詳細は `SDL3/BUILD-*.md` を参照してください。
