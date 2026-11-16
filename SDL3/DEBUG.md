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
| `info` | シーン・ウィンドウ・フレームバッファの大きさ、FPS、カーソルの位置を出す |
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

## 確認

Windows (NVIDIA、OpenGL ES 3.2) で、`info` / `move` / `click` (左右) / `key` / `capture` / `log` / `quit` が動くこと、
`key Escape` で既定の終了処理が動くこと、1 フレーム以内のクリック・キー入力が `down()` で 1 回ずつ数えられることを確認しました。
