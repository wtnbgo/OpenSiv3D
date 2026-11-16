# OpenSiv3D (SDL3 版) のログと簡易 REPL (開発・自動テスト用)

コンソールの無い GUI アプリ (Windows など) のログを読んだり、エージェントやスクリプトからアプリを操作して
動作を確かめたりするための仕組みです。どちらも **環境変数を設定したときだけ** 有効で、通常の実行には影響しません。

## ログファイル: `SIV3D_LOG_FILE`

```bash
SIV3D_LOG_FILE=/tmp/siv3d.log ./MyApp
```

エンジンのログ (`Logger` / `LOG_INFO` など。通常は標準エラーに出る) を、指定したファイルにも UTF-8 で書きます。
起動のたびに作り直し、1 行ごとに書き出すので、異常終了しても直前までのログが残ります。

- Windows の GUI アプリは標準エラーをどこにも出さないので、ログを読むにはこれを使います。
- Android は logcat (`adb logcat -s Siv3D`)、Web はブラウザのコンソールに出ます (この環境変数は使いません)。

## 簡易 REPL: `SIV3D_REPL_FILE`

```bash
SIV3D_LOG_FILE=/tmp/siv3d.log SIV3D_REPL_FILE=/tmp/repl.txt ./MyApp
```

アプリは毎フレーム、指定したファイルがあるかを見て、あれば 1 行ずつ命令を実行し、ファイルを消します
(消えたら受け付けた合図)。実行結果はログに `[repl]` を付けて出るので、`SIV3D_LOG_FILE` と組み合わせて読みます。

**書き込み途中のファイルを読まないよう、別名で書いてから名前を変えて置いてください。**

```bash
printf 'info\ncapture /tmp/shot.png\n' > /tmp/repl.tmp && mv /tmp/repl.tmp /tmp/repl.txt
```

| 命令 | 内容 |
|---|---|
| `capture <パス>` | 画面を保存する (このフレームの描画の後。拡張子で形式が決まる。相対パスは実行ファイルの隣の `Screenshot/` が基準) |
| `move <x> <y>` | マウスカーソルを動かす (シーンの座標。拡大表示の余白も考慮する)。Wayland のようにポインタがウィンドウ上に無いと OS のカーソルを動かせない環境でも、実際のマウスが動くまでは `Cursor::Pos()` がこの位置になる |
| `click <x> <y> [l\|r\|m]` | その位置をクリックする (押して離す。1 フレーム以内でも `MouseL.down()` が反応する) |
| `key <キー名>` | キーを押して離す (SDL のキー名: `A` / `Space` / `Return` / `Escape` / `F1` / `Left` など) |
| `text <文字列>` | 文字の入力 (IME の確定) を送る (`SDL_EVENT_TEXT_INPUT`。日本語も可。`TextInput::UpdateText()` や `SimpleGUI::TextBox` に入る) |
| `editing [文字列]` | IME の変換中の文字を送る (`SDL_EVENT_TEXT_EDITING`。`TextInput::GetEditingText()` に入る。空で変換中を消す) |
| `info` | シーン・ウィンドウ・フレームバッファの大きさ、FPS、カーソルの位置、文字入力を受け付けているか (`text input active`) を出す |
| `log <文字列>` | ログに目印を書く |
| `quit` | アプリを終了する (`System::Exit()`) |
| `# ...` | コメント |

- 命令はウィンドウのイベント処理の前に実行され、押したクリックやキーはそのフレームで反映されます。
- Windows / macOS / Linux で使えます (Web・Android・iOS ではファイルを外から置けないので実質使えません)。
- PowerShell の例:

  ```powershell
  $env:SIV3D_LOG_FILE = "$PWD\siv3d.log"; $env:SIV3D_REPL_FILE = "$PWD\repl.txt"
  $p = Start-Process .\MyApp.exe -PassThru
  Set-Content repl.tmp "click 400 300`ncapture $PWD\shot.png" -Encoding UTF8; Move-Item -Force repl.tmp repl.txt
  ```

## マスターリリースでは REPL を外す: `SIV3D_WITH_REPL`

REPL は外から命令を送り込める仕組みなので、**マスターリリースのビルドでは CMake のオプション `SIV3D_WITH_REPL=OFF` で
コードごと外してください** (既定は ON。OFF にすると `SIV3D_REPL_FILE` を設定しても何も起きない)。ログファイル
(`SIV3D_LOG_FILE`) は操作を受け付けないので、オプションに関係なく使えます。

テンプレート siv3d_app では、マスターリリースをビルドする環境の `siv3d-app.local.json` (または `siv3d-app.json`) に書きます:

```json
{ "cmake": { "options": { "SIV3D_WITH_REPL": "OFF" } } }
```

- CMake のオプションはビルドツリーのキャッシュに残るので、OFF でビルドした後に開発用に戻すときは、`"ON"` と明示して
  configure し直すか、ビルドツリー (`build/<機種>/cmake`) を消してください (指定を消すだけでは OFF のまま)。
- 切り替えるとエンジン全体がビルドし直しになります。マスターリリース用には、開発用とは別のフォルダ
  (テンプレートを別の場所に取り出したものなど) でビルドすると、行き来の手間がかかりません。

## 確認

Windows (NVIDIA、OpenGL ES 3.2) で、`info` / `move` / `click` (左右) / `key` / `capture` / `log` / `quit` が動くこと、
`key Escape` で既定の終了処理が動くこと、1 フレーム以内のクリック・キー入力が `down()` で 1 回ずつ数えられることを確認しました。
`SIV3D_WITH_REPL=OFF` では REPL のソースがビルドに含まれず、エンジンのライブラリにも REPL のコードが入らないことを確認しました。
