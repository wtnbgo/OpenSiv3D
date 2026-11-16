#!/usr/bin/env python3
# -----------------------------------------------------------------------------
# Siv3D (SDL3 版) アプリのビルド・配布物作成ドライバ (標準ライブラリのみ、Python 3.9 以上)
#
#   python <SIV3D_BASE>/tools/siv3d_app.py [options] <command>...
#
# コマンド:
#   configure   CMake の configure (<project>/build/<target>/cmake)
#   build       ビルド (必要なら configure も行う)
#   stage       配布用の構成を作る (cmake --install → <project>/build/<target>/dist)
#   package     配布物を作る (<project>/build/<target>/package/<exeName>-<version>-<target>.zip など)
#   all         build + stage + package
#   run         stage した実行ファイルを起動する
#   clean       <project>/build/<target> を削除する
#   info        設定 (siv3d-app.json と解決したパス) を表示する
#
# オプション:
#   --project DIR    アプリのフォルダ (siv3d-app.json がある場所。既定: カレントディレクトリ)
#   --target T       windows / linux / macos / ios / web / android (既定: 実行中の OS)
#   --sdk S          ios: simulator (既定。ホストの CPU 向け) / device (arm64 の実機)
#   --device ID      ios: run --sdk device でインストールする実機の UDID または名前
#                    (既定: siv3d-app.local.json の ios.device、環境変数 SIV3D_IOS_DEVICE、無ければ接続中の 1 台)
#
# 開発者ごとの設定 (コミットしない):
#   <project>/siv3d-app.local.json があれば siv3d-app.json に重ねて読む (例: { "ios": { "team": "ABCDE12345" } })。
#   環境変数 SIV3D_IOS_TEAM は ios.team を、SIV3D_IOS_DEVICE は ios.device を上書きする
#   --config C       Release / Debug / RelWithDebInfo (既定: Release)
#   --native         linux: コンテナを使わずホストで直接ビルドする (出力は build/linux-native)
#   --container IMG  linux: ビルドに使うコンテナイメージ (既定: siv3d-app.json の linux.container)
#   --engine E       linux: コンテナの実行に使うコマンド auto / docker / wslc / podman
#                    (既定: siv3d-app.json の linux.engine、無ければ auto =
#                     docker のデーモンが動いていれば docker、Windows で動いていなければ wslc)
#
# 機種とビルドするホスト:
#   windows : Windows (Visual Studio 2022 以降。開発者環境でなければ vswhere で探して読み込む)
#   macos   : macOS (Xcode Command Line Tools)
#   ios     : macOS (Xcode)。CMake の Xcode ジェネレータでビルドし、simulator は simctl で起動する。
#             package は simulator なら .app の zip、device なら .ipa (署名は ios.team と Xcode の自動署名)
#   web     : どのホストでも (emsdk。環境変数 EMSDK)。出力は <exeName>.html / .js / .wasm / .data と serve.py
#   android : どのホストでも (Android SDK / NDK / JDK 17 以上)。外枠 siv3d_and (OpenSiv3D の隣、または
#             環境変数 SIV3D_ANDROID) の Gradle でビルドする。出力は <exeName>.apk (package で aab も可)
#   linux   : sniper SDK のコンテナ (steamdev の deckbuild-sniper イメージ。SDL3/BUILD_LINUX.md を参照)。
#             Windows は docker または WSL のコンテナ (wslc)、macOS / Linux は docker。
#             Linux ホストでは --native でホストのコンパイラでもビルドできる
# -----------------------------------------------------------------------------

import argparse
import json
import os
import platform
import shutil
import subprocess
import sys
import tarfile
import zipfile
from pathlib import Path

SIV3D_BASE = Path(__file__).resolve().parent.parent
TARGETS = ("windows", "linux", "macos", "ios", "web", "android")


def log(message):
    print(f"[siv3d_app] {message}", flush=True)


def fail(message):
    print(f"[siv3d_app] error: {message}", file=sys.stderr, flush=True)
    sys.exit(1)


def host_target():
    system = platform.system()
    if system.startswith(("MSYS", "MINGW", "CYGWIN")):
        # msys2 / Cygwin の Python はパスが /c/... になり、MSVC や CMake (Windows 版) と合わない
        # (msys2 の make を使うと PATH の先頭の msys2 の python が選ばれることがある)
        fail(f"unsupported Python ({system}, {sys.executable}): use the Windows Python "
             "(python.org / py launcher / scoop), e.g. make PYTHON=py")
    if system == "Windows":
        return "windows"
    if system == "Darwin":
        return "macos"
    return "linux"


def deep_merge(base, override):
    """辞書を再帰的に重ねる (override が優先。辞書以外の値は置き換える)"""
    result = dict(base)
    for key, value in override.items():
        if isinstance(value, dict) and isinstance(result.get(key), dict):
            result[key] = deep_merge(result[key], value)
        else:
            result[key] = value
    return result


def run(cmd, cwd=None, env=None):
    cmd = [str(c) for c in cmd]
    log("$ " + " ".join(cmd))
    if env is not None:
        # Windows の subprocess は実行ファイルを親プロセスの PATH で探すので、渡す環境の PATH で解決しておく
        path = next((v for k, v in env.items() if k.upper() == "PATH"), None)
        exe = shutil.which(cmd[0], path=path)
        if exe:
            cmd[0] = exe
    result = subprocess.run(cmd, cwd=cwd, env=env)
    if result.returncode != 0:
        fail(f"command failed ({result.returncode})")


# -----------------------------------------------------------------------------
# 設定
# -----------------------------------------------------------------------------

class App:
    def __init__(self, project_dir, target, config, sdk="simulator", build_name=None):
        self.project_dir = Path(project_dir).resolve()
        self.target = target
        self.config = config
        self.sdk = sdk
        self.device = ""

        config_path = self.project_dir / "siv3d-app.json"
        if not config_path.exists():
            fail(f"siv3d-app.json not found in {self.project_dir}")
        with open(config_path, encoding="utf-8") as f:
            self.json = json.load(f)

        # 開発者ごとの設定 (Team ID など。git の管理外にする) を重ねる
        local_path = self.project_dir / "siv3d-app.local.json"
        if local_path.exists():
            with open(local_path, encoding="utf-8") as f:
                self.json = deep_merge(self.json, json.load(f))
        ios = self.json.setdefault("ios", {})
        if os.environ.get("SIV3D_IOS_TEAM"):
            ios["team"] = os.environ["SIV3D_IOS_TEAM"]
        if os.environ.get("SIV3D_IOS_DEVICE"):
            ios["device"] = os.environ["SIV3D_IOS_DEVICE"]

        self.exe_name = self.json.get("exeName", self.project_dir.name)
        self.version = self.json.get("version", "1.0.0")
        self.target_json = self.json.get(target, {})

        # linux: ホストで直接ビルドする場合 (--native) はコンテナのビルドツリー (パスが /app/... ) と
        # 混ざらないよう build/linux-native を使う
        self.build_dir = self.project_dir / "build" / (build_name or target)
        self.native_host_build = (build_name == "linux-native")
        if target == "ios":
            # simulator はホストの CPU (Intel Mac は x86_64、Apple Silicon は arm64)、device は arm64
            self.ios_arch = "arm64" if sdk == "device" else ("x86_64" if platform.machine() == "x86_64" else "arm64")
            self.ios_sysroot = "iphoneos" if sdk == "device" else "iphonesimulator"
            self.build_dir = self.project_dir / "build" / "ios" / f"{sdk}-{self.ios_arch}"
        self.cmake_dir = self.build_dir / "cmake"
        self.dist_dir = self.build_dir / "dist"
        self.package_dir = self.build_dir / "package"

    def cmake_options(self):
        options = dict(self.json.get("cmake", {}).get("options", {}))
        options.update(self.target_json.get("cmake", {}).get("options", {}))
        return [f"-D{k}={v}" for k, v in options.items()]

    def packages(self):
        if self.target == "ios":
            return self.target_json.get("package", ["ipa"] if self.sdk == "device" else ["zip"])
        default = {"windows": ["zip"], "linux": ["tar.gz"], "macos": ["zip"], "web": ["zip"], "android": ["apk"]}[self.target]
        return self.target_json.get("package", default)

    def executable(self):
        if self.target == "android":
            return self.dist_dir / f"{self.exe_name}.apk"
        if self.target == "web":
            return self.dist_dir / f"{self.exe_name}.html"
        if self.target == "windows":
            return self.dist_dir / f"{self.exe_name}.exe"
        if self.target == "macos":
            return self.dist_dir / f"{self.exe_name}.app" / "Contents" / "MacOS" / self.exe_name
        if self.target == "ios":
            return self.dist_dir / f"{self.exe_name}.app"
        return self.dist_dir / self.exe_name


# -----------------------------------------------------------------------------
# Windows: Visual Studio の開発者環境
# -----------------------------------------------------------------------------

def msvc_env():
    """cl.exe が使える環境変数を返す (開発者環境でなければ vcvars64.bat を読み込む)"""
    if shutil.which("cl") and shutil.which("ninja"):
        return None

    vswhere = Path(os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.exists():
        fail("Visual Studio not found (vswhere.exe). Run from a developer command prompt.")
    install = subprocess.run([str(vswhere), "-latest", "-products", "*", "-requires",
                              "Microsoft.VisualStudio.Component.VC.Tools.x86.x64",
                              "-property", "installationPath"],
                             capture_output=True, text=True).stdout.strip().splitlines()
    if not install:
        fail("Visual Studio with C++ tools not found")
    vcvars = Path(install[0]) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
    log(f"using {vcvars}")

    # cmd /U: 内部コマンド (set) の出力を UTF-16 にする。コードページ (cp932 など) で読むと、
    # 環境変数に含まれる UTF-8 などのバイト列で UnicodeDecodeError になることがある
    raw = subprocess.run(f'cmd /U /C ""{vcvars}" >nul && set"', capture_output=True).stdout
    output = raw.decode("utf-16-le", errors="replace")
    env = {}
    for line in output.splitlines():
        if "=" in line:
            key, value = line.split("=", 1)
            env[key] = value
    return env


# -----------------------------------------------------------------------------
# コマンド
# -----------------------------------------------------------------------------

def check_vcpkg():
    if not os.environ.get("VCPKG_ROOT"):
        fail("VCPKG_ROOT is not set")


def configure(app, env):
    check_vcpkg()
    if app.target == "ios":
        deployment = app.target_json.get("deploymentTarget", "15.0")
        cmd = ["cmake", "-S", app.project_dir, "-B", app.cmake_dir,
               "-G", "Xcode",
               "-DCMAKE_SYSTEM_NAME=iOS",
               f"-DCMAKE_OSX_SYSROOT={app.ios_sysroot}",
               f"-DCMAKE_OSX_ARCHITECTURES={app.ios_arch}",
               f"-DCMAKE_OSX_DEPLOYMENT_TARGET={deployment}",
               f"-DSIV3D_BASE={SIV3D_BASE.as_posix()}",
               # siv3d-app.local.json / 環境変数で上書きした値を渡す (空なら siv3d-app.json の値を使う)
               f"-DSIV3D_IOS_TEAM={app.target_json.get('team', '')}",
               f"-DSIV3D_IOS_BUNDLE_ID_OVERRIDE={ios_bundle_id(app)}"]
        cmd += app.cmake_options()
        run(cmd, env=env)
        return
    cmd = ["cmake", "-S", app.project_dir, "-B", app.cmake_dir,
           "-G", "Ninja Multi-Config",
           f"-DSIV3D_BASE={SIV3D_BASE.as_posix()}"]
    if app.target == "web":
        # Emscripten: vcpkg の triplet と toolchain (-pthread / -fwasm-exceptions を足すラッパー)
        if not os.environ.get("EMSDK"):
            fail("EMSDK is not set (install emsdk and set EMSDK to its folder)")
        cmd += ["-DVCPKG_TARGET_TRIPLET=wasm32-emscripten",
                f"-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE={(SIV3D_BASE / 'cmake' / 'toolchains' / 'wasm32-emscripten.cmake').as_posix()}"]
    cmd += app.cmake_options()
    run(cmd, env=env)


def require_ios_team(app):
    """実機向けのビルドには署名用の Team ID が要る (configure の前に確認する)"""
    if app.target == "ios" and app.sdk == "device" and not app.target_json.get("team"):
        fail("ios.team (Apple Developer Team ID) is required for --sdk device. "
             "Set it in siv3d-app.local.json ({ \"ios\": { \"team\": \"ABCDE12345\" } }) or SIV3D_IOS_TEAM")


def build(app, env):
    require_ios_team(app)
    # configure が途中で失敗・中断したビルドツリー (CMakeCache.txt はあるが build.ninja が無い) は、
    # CMakeCache.txt だけ見るとビルドに失敗し続ける。そのまま configure し直しても、vcpkg が依存の
    # インストール済みと判断して飛ばす (find_package の結果もキャッシュに残る) ので、キャッシュを消してやり直す
    # (ios は Xcode ジェネレータなので build.ninja の代わりに .xcodeproj を見る)
    generated = (any(app.cmake_dir.glob("*.xcodeproj")) if app.target == "ios"
                 else (app.cmake_dir / "build.ninja").exists())
    if not generated:
        cache = app.cmake_dir / "CMakeCache.txt"
        if cache.exists():
            log("incomplete build tree (no build.ninja): removing CMakeCache.txt and reconfiguring")
            cache.unlink()
            shutil.rmtree(app.cmake_dir / "CMakeFiles", ignore_errors=True)
        configure(app, env)
    elif app.target == "ios":
        # Xcode ジェネレータは、CMakeLists.txt などが変わると xcodebuild の中 (ZERO_CHECK) で CMake を再実行する。
        # その環境では vcpkg のホスト用ツールが iOS 向けにビルドされてしまうことがあるので、先にここで configure しておく
        configure(app, env)
    cmd = ["cmake", "--build", app.cmake_dir, "--config", app.config]
    if app.target == "ios":
        # xcodebuild に SDK とアーキテクチャを渡す (署名が要る実機は自動でプロファイルを作らせる)
        cmd += ["--", "-sdk", app.ios_sysroot, "-arch", app.ios_arch]
        if app.sdk == "device":
            # 自動署名: 開発用のプロファイルを作り、接続中の実機を Team に登録させる
            # (Xcode の Settings > Accounts に、その Team の Apple ID でサインインしておく必要がある)
            cmd += ["-allowProvisioningUpdates", "-allowProvisioningDeviceRegistration"]
    run(cmd, env=env)


def stage(app, env):
    if app.dist_dir.exists():
        shutil.rmtree(app.dist_dir)
    if app.target == "ios":
        # Xcode のビルド結果 (<config>-iphonesimulator/<exe>.app など) をそのまま置く
        # (出力先を指定しない場合は <config>-iphonesimulator/、指定した場合は <出力先>/<config>/ に置かれる)
        built = (next(app.cmake_dir.rglob(f"{app.config}-{app.ios_sysroot}/{app.exe_name}.app"), None)
                 or next(app.cmake_dir.rglob(f"{app.config}/{app.exe_name}.app"), None))
        if built is None:
            fail(f"built app not found under {app.cmake_dir}")
        app.dist_dir.mkdir(parents=True)
        shutil.copytree(built, app.dist_dir / f"{app.exe_name}.app", symlinks=True)
        log(f"staged: {app.dist_dir}")
        return
    run(["cmake", "--install", app.cmake_dir, "--config", app.config, "--prefix", app.dist_dir], env=env)

    if app.target == "macos":
        # install で RPATH を書き換えた後に署名し直す。既定は ad-hoc 署名 ("-"。Apple Silicon では
        # 署名の無い実行ファイルは起動できない)。配布用の Developer ID は macos.sign に指定する
        # (公証は別途。false で署名しない)
        identity = app.target_json.get("sign", "-")
        if identity:
            run(["codesign", "--force", "--deep", "--sign", identity,
                 app.dist_dir / f"{app.exe_name}.app"])

    log(f"staged: {app.dist_dir}")


def _tar_normalize_owner(info):
    info.uid = 0
    info.gid = 0
    info.uname = ""
    info.gname = ""
    return info


def package(app):
    if not app.dist_dir.exists():
        fail("not staged yet (run stage first)")
    app.package_dir.mkdir(parents=True, exist_ok=True)
    base = f"{app.exe_name}-{app.version}-{app.target}"
    if app.target == "ios":
        base = f"{app.exe_name}-{app.version}-ios-{app.sdk}"
    elif app.native_host_build:
        # ホストのコンパイラ・glibc に依存するので配布用ではない。コンテナ版と取り違えないよう名前を変える
        base += "-native"
        log("warning: --native build depends on the host glibc / libstdc++ (not for distribution; use the container build)")

    for fmt in app.packages():
        if fmt == "ipa":
            # ipa = Payload/<exe>.app を zip にしたもの (実機へのインストールには ios.team による署名が要る)
            out = app.package_dir / f"{base}.ipa"
            payload = app.package_dir / "Payload"
            if payload.exists():
                shutil.rmtree(payload)
            payload.mkdir()
            shutil.copytree(app.dist_dir / f"{app.exe_name}.app", payload / f"{app.exe_name}.app", symlinks=True)
            if out.exists():
                out.unlink()
            run(["ditto", "-c", "-k", "--sequesterRsrc", "--keepParent", payload, out])
            shutil.rmtree(payload)
        elif fmt == "zip":
            out = app.package_dir / f"{base}.zip"
            if app.target in ("macos", "ios"):
                # .app はシンボリックリンクと実行属性を保つ ditto で固める
                run(["ditto", "-c", "-k", "--sequesterRsrc", "--keepParent",
                     app.dist_dir / f"{app.exe_name}.app", out])
            else:
                with zipfile.ZipFile(out, "w", zipfile.ZIP_DEFLATED) as z:
                    for path in sorted(app.dist_dir.rglob("*")):
                        if path.is_file():
                            z.write(path, Path(base) / path.relative_to(app.dist_dir))
        elif fmt in ("tar.gz", "tgz"):
            out = app.package_dir / f"{base}.tar.gz"
            with tarfile.open(out, "w:gz") as t:
                # シンボリックリンク (libSDL3.so.0 など) と実行属性をそのまま保つ。
                # 所有者は作った環境 (コンテナなら root、ホストならビルドした人) によらず
                # uid / gid 0・名前なしにそろえる (tar --owner=0 --group=0 --numeric-owner と同じ)。
                # 展開すると展開した人の所有になる
                t.add(app.dist_dir, arcname=base, filter=_tar_normalize_owner)
        elif fmt == "dir":
            out = app.package_dir / base
            if out.exists():
                shutil.rmtree(out)
            shutil.copytree(app.dist_dir, out, symlinks=True)
        else:
            fail(f"unknown package format: {fmt}")
        log(f"package: {out}")


def ios_bundle_id(app):
    ios = app.json.get("ios", {})
    return ios.get("bundleId", app.json.get("bundleId", f"com.example.{app.exe_name}"))


def run_ios_simulator(app):
    """iOS シミュレータで起動する (起動中のシミュレータが無ければ、利用できる iPhone を 1 台起動する)"""
    bundle = app.executable()
    if not bundle.exists():
        fail(f"not staged yet: {bundle}")
    devices = json.loads(subprocess.run(["xcrun", "simctl", "list", "devices", "available", "--json"],
                                        capture_output=True, text=True).stdout)["devices"]
    all_devices = [d for runtime in devices.values() for d in runtime]
    booted = [d for d in all_devices if d.get("state") == "Booted"]
    if booted:
        udid = booted[0]["udid"]
    else:
        iphones = [d for d in all_devices if d.get("name", "").startswith("iPhone")]
        if not iphones:
            fail("no iOS simulator available (Xcode > Settings > Components)")
        udid = iphones[-1]["udid"]
        run(["xcrun", "simctl", "boot", udid])
    subprocess.run(["open", "-a", "Simulator"])
    run(["xcrun", "simctl", "install", udid, bundle])
    run(["xcrun", "simctl", "launch", "--terminate-running-process", udid, ios_bundle_id(app)])


def ios_connected_devices():
    """devicectl で見える実機の一覧 [(identifier, name, 状態)]"""
    out = Path(subprocess.run(["mktemp"], capture_output=True, text=True).stdout.strip())
    try:
        subprocess.run(["xcrun", "devicectl", "list", "devices", "--quiet", "--json-output", str(out)],
                       capture_output=True, text=True)
        data = json.loads(out.read_text()) if out.exists() and out.stat().st_size else {}
    finally:
        out.unlink(missing_ok=True)
    devices = []
    for d in data.get("result", {}).get("devices", []):
        props = d.get("deviceProperties", {})
        conn = d.get("connectionProperties", {})
        devices.append((d.get("identifier", ""), props.get("name", ""),
                        conn.get("pairingState", ""), conn.get("tunnelState", "")))
    return devices


def run_ios_device(app):
    """実機にインストールして起動する (Xcode 15 以降の devicectl)"""
    bundle = app.executable()
    if not bundle.exists():
        fail(f"not staged yet: {bundle}")
    device = app.device or app.target_json.get("device", "")
    if not device:
        candidates = [d for d in ios_connected_devices() if d[2] == "paired" and d[3] != "unavailable"]
        if not candidates:
            fail("no paired iOS device found (connect the device, trust this Mac, and enable Developer Mode)")
        device = candidates[0][0]
        log(f"device: {candidates[0][1]} ({device})")
    run(["xcrun", "devicectl", "device", "install", "app", "--device", device, bundle])
    run(["xcrun", "devicectl", "device", "process", "launch", "--terminate-existing", "--device", device, ios_bundle_id(app)])


def run_app(app):
    if app.target == "ios":
        if app.sdk == "device":
            run_ios_device(app)
        else:
            run_ios_simulator(app)
        return
    exe = app.executable()
    if not exe.exists():
        fail(f"not staged yet: {exe}")
    if app.target == "web":
        # COOP/COEP ヘッダ付きのサーバで配信する (Ctrl+C で終了)
        port = str(app.target_json.get("port", 8080))
        log(f"open http://127.0.0.1:{port}/{exe.name}")
        subprocess.run([sys.executable, str(app.dist_dir / "serve.py"), port, str(app.dist_dir)])
        return
    log(f"run: {exe}")
    subprocess.run([str(exe)], cwd=exe.parent)


def clean(app):
    if app.build_dir.exists():
        shutil.rmtree(app.build_dir)
        log(f"removed {app.build_dir}")


def info(app):
    print(f"SIV3D_BASE : {SIV3D_BASE}")
    print(f"project    : {app.project_dir}")
    print(f"target     : {app.target} ({app.config})")
    print(f"exeName    : {app.exe_name}")
    print(f"version    : {app.version}")
    print(f"cmake opts : {' '.join(app.cmake_options())}")
    print(f"build      : {app.cmake_dir}")
    print(f"dist       : {app.dist_dir}")
    print(f"package    : {app.package_dir} {app.packages()}")
    if app.target == "ios":
        team = app.target_json.get("team", "")
        print(f"ios team   : {(team[:3] + '*' * (len(team) - 3)) if team else '(not set)'}")
        print(f"ios bundle : {ios_bundle_id(app)}")
        print(f"ios device : {app.device or app.target_json.get('device', '') or '(auto)'}")


# -----------------------------------------------------------------------------
# Linux のコンテナビルド (Windows / macOS ホスト、または Linux ホストで --native 無し)
# -----------------------------------------------------------------------------

def find_wslc():
    exe = shutil.which("wslc")
    if exe:
        return exe
    path = Path(os.environ.get("ProgramFiles", r"C:\Program Files")) / "WSL" / "wslc.exe"
    return str(path) if path.exists() else None


def container_engine(requested):
    """コンテナの実行コマンド (docker / wslc) を返す"""
    if requested and requested != "auto":
        exe = find_wslc() if requested == "wslc" else shutil.which(requested)
        if not exe:
            fail(f"container engine not found: {requested}")
        return exe

    # docker のデーモンが動いていれば docker、Windows で動いていなければ WSL のコンテナ (wslc)
    docker = shutil.which("docker")
    if docker and subprocess.run([docker, "info"], capture_output=True).returncode == 0:
        return docker
    if platform.system() == "Windows":
        wslc = find_wslc()
        if wslc:
            return wslc
    fail("no container engine (docker daemon is not running, wslc not found). Use --native on a Linux host.")


def run_in_container(app, args, commands):
    image = args.container or app.target_json.get("container", "deckbuild-sniper")
    engine = container_engine(args.engine or app.target_json.get("engine"))
    log(f"container: {engine} / {image}")

    # アプリと OpenSiv3D をコンテナの固定パスにマウントし、コンテナ内でこのスクリプトを --native で動かす。
    # コンパイラ・vcpkg のバイナリキャッシュ (名前付きボリューム deckbuild-cache) は steamdev の
    # deckbuild.sh と同じ設定にする (sniper SDK の既定の gcc は古いので gcc-14 を使う)
    inner = " ".join(["mkdir -p /cache/vcpkg-bincache &&",
                      "python3", "/siv3d/tools/siv3d_app.py", "--native", "--in-container", "--target", "linux",
                      "--config", app.config, "--project", "/app"] + commands)
    cmd = [engine, "run", "--rm",
           "-v", f"{app.project_dir}:/app",
           "-v", f"{SIV3D_BASE}:/siv3d",
           "-v", "deckbuild-cache:/cache",
           "-e", "VCPKG_DEFAULT_BINARY_CACHE=/cache/vcpkg-bincache",
           "-e", "CC=gcc-14", "-e", "CXX=g++-14",
           "-w", "/app",
           image, "bash", "-c", inner]
    try:
        run(cmd)
    finally:
        # コンテナは root で動くので、Linux ホストではビルド結果の所有者を戻す
        # (ビルドに失敗したときも戻さないと、ホストから clean できなくなる)
        # <project>/build もコンテナが作るので、build/<target> だけでなく build ごと戻す
        out_dir = app.build_dir.parent
        if platform.system() == "Linux" and hasattr(os, "getuid") and out_dir.exists():
            subprocess.run([engine, "run", "--rm", "-v", f"{out_dir}:/out", image,
                            "chown", "-R", f"{os.getuid()}:{os.getgid()}", "/out"])


# -----------------------------------------------------------------------------
# Android: 外枠 siv3d_and (Gradle) でビルドする
# -----------------------------------------------------------------------------

def android_shell_dir():
    """siv3d_and の場所: 環境変数 SIV3D_ANDROID → OpenSiv3D の隣の siv3d_and"""
    d = Path(os.environ.get("SIV3D_ANDROID", SIV3D_BASE.parent / "siv3d_and")).resolve()
    if not (d / "settings.gradle").exists():
        fail(f"siv3d_and not found: {d} (set SIV3D_ANDROID)")
    return d


def gradle(app, tasks, abis=None):
    shell = android_shell_dir()
    gradlew = shell / ("gradlew.bat" if platform.system() == "Windows" else "gradlew")
    env = dict(os.environ, PROJECT_DIR=str(app.project_dir), SIV3D_BASE=str(SIV3D_BASE))
    cmd = [gradlew, "--console=plain"] + tasks
    if abis:
        cmd.append(f"-Pabis={abis}")
    run(cmd, cwd=shell, env=env)


def android_variant(app):
    return "Debug" if app.config == "Debug" else "Release"


def android_build(app, args):
    gradle(app, [f"assemble{android_variant(app)}"], args.abis)


def android_stage(app):
    variant = android_variant(app).lower()
    apks = sorted((app.build_dir / "gradle" / "outputs" / "apk" / variant).glob("*.apk"))
    if not apks:
        fail("APK not found (run build first)")
    if app.dist_dir.exists():
        shutil.rmtree(app.dist_dir)
    app.dist_dir.mkdir(parents=True)
    shutil.copy2(apks[0], app.executable())
    log(f"staged: {app.executable()}")


def android_package(app, args):
    app.package_dir.mkdir(parents=True, exist_ok=True)
    base = f"{app.exe_name}-{app.version}-android"
    for fmt in app.packages():
        if fmt == "apk":
            out = app.package_dir / f"{base}.apk"
            shutil.copy2(app.executable(), out)
        elif fmt == "aab":
            gradle(app, [f"bundle{android_variant(app)}"], args.abis)
            variant = android_variant(app).lower()
            aabs = sorted((app.build_dir / "gradle" / "outputs" / "bundle" / variant).glob("*.aab"))
            if not aabs:
                fail("AAB not found")
            out = app.package_dir / f"{base}.aab"
            shutil.copy2(aabs[0], out)
        else:
            fail(f"unknown package format for android: {fmt}")
        log(f"package: {out}")


def android_run(app):
    adb = shutil.which("adb") or str(Path(os.environ.get("ANDROID_HOME", "")) / "platform-tools" / "adb")
    android_json = app.target_json
    app_id = android_json.get("applicationId", app.json.get("bundleId", "com.example.siv3dapp"))
    run([adb, "install", "-r", app.executable()])
    run([adb, "shell", "am", "start", "-n", f"{app_id}/org.siv3d.app.SivActivity"])


# -----------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Siv3D (SDL3) app build driver")
    parser.add_argument("commands", nargs="+",
                        choices=["configure", "build", "stage", "package", "all", "run", "clean", "info"])
    parser.add_argument("--project", default=".")
    parser.add_argument("--target", choices=TARGETS, default=host_target())
    parser.add_argument("--config", default="Release")
    parser.add_argument("--sdk", choices=["simulator", "device"], default="simulator")
    parser.add_argument("--device", default="")
    parser.add_argument("--native", action="store_true")
    parser.add_argument("--in-container", action="store_true", help=argparse.SUPPRESS)
    parser.add_argument("--container")
    parser.add_argument("--engine", choices=["auto", "docker", "wslc", "podman"])
    parser.add_argument("--abis", help="android: ABI (カンマ区切り。例: x86_64)。既定は siv3d-app.json の android.abis")
    args = parser.parse_args()

    build_name = "linux-native" if (args.target == "linux" and args.native and not args.in_container) else None
    app = App(args.project, args.target, args.config, args.sdk, build_name)
    app.device = args.device

    commands = []
    for c in args.commands:
        commands += ["build", "stage", "package"] if c == "all" else [c]

    host = host_target()
    if app.target == "ios":
        if host != "macos":
            fail("target 'ios' can be built only on macOS")
    # web / android はどのホストでもビルドできる (emsdk / Android SDK)。linux は他のホストではコンテナでビルドする
    elif app.target != host and app.target not in ("web", "android") and not (app.target == "linux" and not args.native):
        fail(f"target '{app.target}' cannot be built on {host}")

    # linux: ネイティブ指定が無ければコンテナでビルドする (info / clean / run はホストで実行)
    if app.target == "linux" and not args.native and any(c in ("configure", "build", "stage", "package") for c in commands):
        container_cmds = [c for c in commands if c in ("configure", "build", "stage", "package")]
        run_in_container(app, args, container_cmds)
        commands = [c for c in commands if c not in container_cmds]

    # Windows ホストでは、web も CMake / Ninja を Visual Studio の開発者環境から使う
    env = msvc_env() if (app.target in ("windows", "web") and host == "windows") else None

    if app.target == "android":
        for c in commands:
            if c in ("configure", "build"):
                android_build(app, args)
            elif c == "stage":
                android_stage(app)
            elif c == "package":
                android_package(app, args)
            elif c == "run":
                android_run(app)
            elif c == "clean":
                clean(app)
            elif c == "info":
                info(app)
        return

    for c in commands:
        if c == "configure":
            configure(app, env)
        elif c == "build":
            build(app, env)
        elif c == "stage":
            stage(app, env)
        elif c == "package":
            package(app)
        elif c == "run":
            run_app(app)
        elif c == "clean":
            clean(app)
        elif c == "info":
            info(app)


if __name__ == "__main__":
    main()
