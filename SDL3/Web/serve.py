#!/usr/bin/env python3
# Siv3D (SDL3 版) Web 版の確認用サーバ
#
#   python serve.py [port] [directory]     (既定: 8080, このファイルのあるフォルダ)
#
# Web 版はスレッド (SharedArrayBuffer) を使うため、Cross-Origin-Opener-Policy / Cross-Origin-Embedder-Policy
# ヘッダを付けて配信する必要がある (file:// や通常の静的サーバでは動かない)。
import http.server
import os
import sys
from functools import partial


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        ".wasm": "application/wasm",
        ".js": "text/javascript",
        ".data": "application/octet-stream",
    }

    def end_headers(self):
        self.send_header("Cross-Origin-Opener-Policy", "same-origin")
        self.send_header("Cross-Origin-Embedder-Policy", "require-corp")
        self.send_header("Cache-Control", "no-cache")
        super().end_headers()


def main():
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 8080
    directory = sys.argv[2] if len(sys.argv) > 2 else os.path.dirname(os.path.abspath(__file__))
    server = http.server.ThreadingHTTPServer(("127.0.0.1", port), partial(Handler, directory=directory))
    print(f"Serving {directory} at http://127.0.0.1:{port}/")
    server.serve_forever()


if __name__ == "__main__":
    main()
