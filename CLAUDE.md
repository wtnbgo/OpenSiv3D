# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

OpenSiv3D is a C++20/23 framework for creative coding (2D/3D games, media art, visualizers, simulators). Version 0.6.16. The project is actively migrating platform backends to SDL3.

## Build System

**Prerequisites:** CMake 3.20+, vcpkg, Ninja, C++23 compiler.

**Configure and build (Windows):**
```bash
cmake --preset x64-windows
cmake --build build/x64-windows --config Release
```

**Other presets:** `x64-linux`, `osx`, `osx-arm64`, `android-arm64`, `android-x86_64`

All presets use Ninja Multi-Config and vcpkg for dependency management. SDL3 is fetched via CMake FetchContent.

**Key defines:** `SIV3D_USE_SDL3`, `WITH_MINIAUDIO`, `AS_USE_NAMESPACE`, `MUPARSER_STATIC`, `_UNICODE`

**Tests:** Located in `Test/` using Catch2. Built as part of the CMake project.

## Architecture

### Engine Component System

The engine uses an interface-based component architecture defined in `Siv3D/src/Siv3D/Common/Siv3DEngine.hpp`. A central `Siv3DEngine` holds ~50 components via `Siv3DComponent<ISiv3D*>` wrappers in a `std::tuple`.

Each component follows this pattern:
- **Interface:** `ISiv3DXxx` (e.g., `ISiv3DWindow`, `ISiv3DAudio`) — abstract interface
- **Implementation:** `CXxx` (e.g., `CWindow`, `CAudio`) — concrete platform-specific implementation inheriting the interface
- **Factory:** `XxxFactory.cpp` — creates the platform-appropriate implementation at runtime

### Source Layout

- `Siv3D/include/Siv3D/` — Public API headers (~300+ features, one directory per module)
- `Siv3D/src/Siv3D/` — Cross-platform implementation (one directory per module, mirrors include structure)
- `Siv3D/src/Siv3D-Platform/` — Platform-specific implementations:
  - `SDL3/` — New unified SDL3 backend (primary target for new development)
  - `OpenGLES3/` — OpenGL ES 3 renderer implementation
  - `OpenGL4/` — OpenGL 4 renderer (legacy, being phased out)
  - `Windows/`, `macOS/`, `Linux/` — Legacy platform-specific code
  - `macOS_Linux/` — Shared macOS/Linux code
  - `Web/` — Emscripten/WebAssembly backend
  - `WebGPU/` — Experimental WebGPU renderer
- `Siv3D/src/ThirdParty/` — Embedded third-party libraries (Box2D, AngelScript, muparser, etc.)
- `Siv3D/lib/` — Prebuilt platform-specific libraries
- `Siv3D/addon/` — Extension modules (e.g., Multiplayer_Photon)
- `Dependencies/boost_1_83_0/` — Boost headers (for Visual Studio / Xcode builds)

### Rendering Pipeline

Abstracted behind `ISiv3DRenderer`, with separate 2D (`ISiv3DRenderer2D`) and 3D (`ISiv3DRenderer3D`) paths. Current active backend is OpenGL ES 3 (`GLES3*` classes). Renderer state management (blend, sampler, depth/stencil, rasterizer) is encapsulated per backend.

### Namespace

All engine code lives in the `s3d` namespace.

## Branching & Contributing

- `v6_develop` — Development branch (target for PRs)
- `main` — Stable releases
- Coding standards: https://siv3d.github.io/en-us/develop/coding-style/
- Build instructions: https://siv3d.github.io/en-us/develop/build/
- Major changes should be discussed on the Siv3D Discord or GitHub Issues first

## Key Dependencies (via vcpkg)

ZLIB, libpng, libjpeg, libtiff, Opus, Vorbis, WebP, CURL, Boost, FreeType, HarfBuzz, GIF, OpenCV 4. SDL3 is fetched directly via FetchContent.

## Language Notes

- Commit messages and some documentation are in Japanese
- Public API documentation is bilingual (English/Japanese)
