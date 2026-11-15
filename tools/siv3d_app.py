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
#   --target T       windows / linux / macos / ios (既定: 実行中の OS)
#   --sdk S          ios: simulator (既定。ホストの CPU 向け) / device (arm64 の実機)
#   --config C       Release / Debug / RelWithDebInfo (既定: Release)
#   --native         linux: コンテナを使わずホストで直接ビルドする
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
TARGETS = ("windows", "linux", "macos", "ios")


def log(message):
    print(f"[siv3d_app] {message}", flush=True)


def fail(message):
    print(f"[siv3d_app] error: {message}", file=sys.stderr, flush=True)
    sys.exit(1)


def host_target():
    system = platform.system()
    if system == "Windows":
        return "windows"
    if system == "Darwin":
        return "macos"
    return "linux"


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
    def __init__(self, project_dir, target, config, sdk="simulator"):
        self.project_dir = Path(project_dir).resolve()
        self.target = target
        self.config = config
        self.sdk = sdk

        config_path = self.project_dir / "siv3d-app.json"
        if not config_path.exists():
            fail(f"siv3d-app.json not found in {self.project_dir}")
        with open(config_path, encoding="utf-8") as f:
            self.json = json.load(f)

        self.exe_name = self.json.get("exeName", self.project_dir.name)
        self.version = self.json.get("version", "1.0.0")
        self.target_json = self.json.get(target, {})

        self.build_dir = self.project_dir / "build" / target
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
        default = {"windows": ["zip"], "linux": ["tar.gz"], "macos": ["zip"]}[self.target]
        return self.target_json.get("package", default)

    def executable(self):
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

    output = subprocess.run(f'"{vcvars}" >nul && set', shell=True, capture_output=True, text=True).stdout
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
               f"-DSIV3D_BASE={SIV3D_BASE.as_posix()}"]
        cmd += app.cmake_options()
        run(cmd, env=env)
        return
    cmd = ["cmake", "-S", app.project_dir, "-B", app.cmake_dir,
           "-G", "Ninja Multi-Config",
           f"-DSIV3D_BASE={SIV3D_BASE.as_posix()}"]
    cmd += app.cmake_options()
    run(cmd, env=env)


def build(app, env):
    if not (app.cmake_dir / "CMakeCache.txt").exists():
        configure(app, env)
    cmd = ["cmake", "--build", app.cmake_dir, "--config", app.config]
    if app.target == "ios":
        # xcodebuild に SDK とアーキテクチャを渡す (署名が要る実機は自動でプロファイルを作らせる)
        cmd += ["--", "-sdk", app.ios_sysroot, "-arch", app.ios_arch]
        if app.sdk == "device":
            cmd += ["-allowProvisioningUpdates"]
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


def package(app):
    if not app.dist_dir.exists():
        fail("not staged yet (run stage first)")
    app.package_dir.mkdir(parents=True, exist_ok=True)
    base = f"{app.exe_name}-{app.version}-{app.target}"
    if app.target == "ios":
        base = f"{app.exe_name}-{app.version}-ios-{app.sdk}"

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
                # シンボリックリンク (libSDL3.so.0 など) と実行属性をそのまま保つ
                t.add(app.dist_dir, arcname=base)
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


def run_app(app):
    if app.target == "ios":
        if app.sdk != "simulator":
            fail("run supports only --sdk simulator (install to a device with Xcode or devicectl)")
        run_ios_simulator(app)
        return
    exe = app.executable()
    if not exe.exists():
        fail(f"not staged yet: {exe}")
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
                      "python3", "/siv3d/tools/siv3d_app.py", "--native", "--target", "linux",
                      "--config", app.config, "--project", "/app"] + commands)
    cmd = [engine, "run", "--rm",
           "-v", f"{app.project_dir}:/app",
           "-v", f"{SIV3D_BASE}:/siv3d",
           "-v", "deckbuild-cache:/cache",
           "-e", "VCPKG_DEFAULT_BINARY_CACHE=/cache/vcpkg-bincache",
           "-e", "CC=gcc-14", "-e", "CXX=g++-14",
           "-w", "/app",
           image, "bash", "-c", inner]
    run(cmd)

    # コンテナは root で動くので、Linux ホストではビルド結果の所有者を戻す
    if platform.system() == "Linux" and hasattr(os, "getuid"):
        run([engine, "run", "--rm", "-v", f"{app.build_dir}:/out", image,
             "chown", "-R", f"{os.getuid()}:{os.getgid()}", "/out"])


# -----------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(description="Siv3D (SDL3) app build driver")
    parser.add_argument("commands", nargs="+",
                        choices=["configure", "build", "stage", "package", "all", "run", "clean", "info"])
    parser.add_argument("--project", default=".")
    parser.add_argument("--target", choices=TARGETS, default=host_target())
    parser.add_argument("--config", default="Release")
    parser.add_argument("--sdk", choices=["simulator", "device"], default="simulator")
    parser.add_argument("--native", action="store_true")
    parser.add_argument("--container")
    parser.add_argument("--engine", choices=["auto", "docker", "wslc", "podman"])
    args = parser.parse_args()

    app = App(args.project, args.target, args.config, args.sdk)

    commands = []
    for c in args.commands:
        commands += ["build", "stage", "package"] if c == "all" else [c]

    host = host_target()
    if app.target == "ios":
        if host != "macos":
            fail("target 'ios' can be built only on macOS")
    elif app.target != host and not (app.target == "linux" and not args.native):
        fail(f"target '{app.target}' cannot be built on {host}")

    # linux: ネイティブ指定が無ければコンテナでビルドする (info / clean / run はホストで実行)
    if app.target == "linux" and not args.native and any(c in ("configure", "build", "stage", "package") for c in commands):
        container_cmds = [c for c in commands if c in ("configure", "build", "stage", "package")]
        run_in_container(app, args, container_cmds)
        commands = [c for c in commands if c not in container_cmds]

    env = msvc_env() if (app.target == "windows" and host == "windows") else None

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
