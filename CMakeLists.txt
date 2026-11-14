cmake_minimum_required(VERSION 3.20)

add_compile_options("$<$<AND:$<C_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:C,CXX>>:/utf-8>")
add_compile_options("$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/Zc:__cplusplus>")
# ignore warnings
add_compile_options("$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:C,CXX>>:/wd4819>")
add_compile_definitions("$<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:C,CXX>>:_CRT_SECURE_NO_WARNINGS>")
# force output PDB
add_link_options("$<$<AND:$<C_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:C,CXX>>:/DEBUG>")

project(OpenSiv3D
    VERSION 0.6.16
    HOMEPAGE_URL "https://github.com/Siv3D/OpenSiv3D"
    DESCRIPTION "A C++20 framework for creative coding (2D/3D games, media art, visualizers, and simulators)."
)

# Find required packages
find_package(ZLIB REQUIRED)
find_package(PNG REQUIRED)
find_package(JPEG REQUIRED)
find_package(TIFF REQUIRED)
find_package(Opus REQUIRED)
find_package(OpusFile CONFIG REQUIRED)
find_package(Vorbis REQUIRED)
find_package(WebP CONFIG REQUIRED)
find_package(CURL CONFIG REQUIRED)
find_package(Boost REQUIRED)
find_package(Freetype REQUIRED)
find_package(harfbuzz CONFIG REQUIRED)
find_package(GIF REQUIRED)
find_package(libjpeg-turbo CONFIG QUIET)
# find_package(unofficial-mpg123 CONFIG REQUIRED)
# find_package(SoundTouch CONFIG REQUIRED)
# find_package(FFmpeg REQUIRED) # vcpkg's ffmpeg might need specific handling or pkg-config

set(OpenCV_ROOT "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share/opencv4")
find_package(OpenCV REQUIRED)
include_directories(${OpenCV_INCLUDE_DIRS}) # Not needed for CMake >= 2.8.11

if(UNIX AND NOT APPLE)
  # Linux: GTK3 (nativefiledialog-extended) と GIO (FileSystem::Remove のゴミ箱) は
  # vcpkg ではなくシステム (sniper SDK) のものを動的リンクする。
  # どちらも Steam Linux Runtime / SteamOS / 一般的なデスクトップ環境に必ずあり、
  # vcpkg で gtk3 一式を静的に抱えるより互換性が高い。
  find_package(PkgConfig REQUIRED)
  pkg_check_modules(SIV3D_LINUX_GTK REQUIRED IMPORTED_TARGET gtk+-3.0 gio-2.0 glib-2.0)
endif()

# ---------------------------------------------
# SDL3
# ---------------------------------------------

include(FetchContent)
FetchContent_Declare(
  SDL3
  GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
  GIT_TAG main
)
set(SDL_SHARED ON CACHE BOOL "Build SDL shared library")
set(SDL_STATIC OFF CACHE BOOL "Build SDL static library")
FetchContent_MakeAvailable(SDL3)





include(GNUInstallDirs)

# C flags
#set(SIV3D_C_FLAGS -Wall -Wextra -Wno-missing-field-initializers -fPIC -msse4.1)
#set(SIV3D_C_FLAGS_DEBUG -g3 -O0 -pg -DDEBUG)
#set(SIV3D_C_FLAGS_RELEASE -O2 -DNDEBUG -march=x86-64 -mtune=generic)
#set(SIV3D_C_FLAGS_RELWITHDEBINFO -g3 -Og -pg)
#set(SIV3D_C_FLAGS_MINSIZEREL -Os -DNDEBUG -march=x86-64 -mtune=generic)
set(SIV3D_C_DEFINITION
    SIV3D_USE_SDL3
    WITH_MINIAUDIO
    WITH_NOSOUND
)

# C++ flags
#set(SIV3D_CXX_FLAGS -Wall -Wextra -Wno-unknown-pragmas -fPIC -msse4.1)
#set(SIV3D_CXX_FLAGS_DEBUG -g3 -O0 -pg -DDEBUG)
#set(SIV3D_CXX_FLAGS_RELEASE -O2 -DNDEBUG -march=x86-64 -mtune=generic)
#set(SIV3D_CXX_FLAGS_RELWITHDEBINFO -g3 -Og -pg)
#set(SIV3D_CXX_FLAGS_MINSIZEREL -Os -DNDEBUG -march=x86-64 -mtune=generic)
set(SIV3D_CXX_DEFINITION
    AS_USE_NAMESPACE
    SIV3D_USE_SDL3
    MUPARSER_STATIC
    _UNICODE
    WITH_MINIAUDIO
    WITH_NOSOUND
)

if("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
    set(SIV3D_CXX_FLAGS_WITH_COMPILER -fcolor-diagnostics)
elseif("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU")
    set(SIV3D_CXX_FLAGS_WITH_COMPILER -fdiagnostics-color=always)
endif()

if (NOT CMAKE_CONFIGURATION_TYPES AND 
    NOT CMAKE_NO_BUILD_TYPE AND
    NOT CMAKE_BUILD_TYPE AND
    CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    message(STATUS "[!] Setting build type to 'Release' as none was specified.")
    set(CMAKE_BUILD_TYPE Release)
endif()

## -------------------------------------------------
## AudioFormat decoders: reuse WindowsDesktop implementations
## These delegate decoding to ISiv3DAudioCodec and contain no
## platform-specific code (header-signature detection + AudioCodec dispatch).
## [TODO] クロスプラットフォーム化する場合は SDL3/ 以下に共通実装を配置する。
##        現状は Windows: Media Foundation, Linux: FFmpeg と
##        AudioCodec バックエンドが分かれているため、
##        SDL_mixer や miniaudio ベースの共通 AudioCodec を用意すれば
##        デコーダー実装もプラットフォーム非依存にできる。
## -------------------------------------------------
set(SIV3D_SDL3_AUDIO_DECODERS
  Siv3D/src/Siv3D-Platform/WindowsDesktop/Siv3D/AudioFormat/MP3/MP3Decoder.cpp
  Siv3D/src/Siv3D-Platform/WindowsDesktop/Siv3D/AudioFormat/AAC/AACDecoder.cpp
  Siv3D/src/Siv3D-Platform/WindowsDesktop/Siv3D/AudioFormat/WMA/WMADecoder.cpp
  Siv3D/src/Siv3D-Platform/WindowsDesktop/Siv3D/AudioFormat/FLAC/FLACDecoder.cpp
)

set(SIV3D_SDL3_SOURCES
  ${SIV3D_SDL3_AUDIO_DECODERS}
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Audio/NullSoundTouchFunctions.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/AudioCodec/AudioCodecFactory.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/AudioCodec/CAudioCodec.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/BinaryReader/BinaryReaderDetail.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/BinaryWriter/BinaryWriterDetail.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/ChildProcess/ChildProcessDetail.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Clipboard/CClipboard.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Common/OpenGL.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Console/CConsole.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/ConstantBuffer/ConstantBufferDetailFactory.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Cursor/CCursor.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/DLL/SivDLL.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Date/SivDate_SDL3.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/DateTime/SivDateTime_SDL3.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Dialog/SivDialog_SDL3.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/DirectoryWatcher/DirectoryWatcherDetail.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/DragDrop/CDragDrop.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/DriveInfo/SivDriveInfo.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/EnvironmentVariable/SivEnvironmentVariable.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/FileSystem/SivFileSystem_SDL3.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/FreestandingMessageBox/FreestandingMessageBox_SDL3.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Gamepad/CGamepad.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Keyboard/CKeyboard.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Logger/CLogger.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/MemoryMappedFile/MemoryMappedFileDetail.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/MemoryMappedFileView/MemoryMappedFileViewDetail.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Mesh/MeshFactory.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/MessageBox/SivMessageBox_SDL3.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Monitor/SivMonitor_SDL3.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Monitor/SivMonitor_SDL3_Base.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Mouse/CMouse.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Network/SivNetwork_SDL3.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Pentablet/PentabletFactory.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/PowerStatus/SivPowerStatus.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Renderer/GLES3/CRenderer_GLES3.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Renderer/RendererFactory.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Renderer2D/Renderer2DFactory.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Renderer3D/Renderer3DFactory.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Resource/CResource.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Resource/SivResource_SDL3.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Shader/ShaderFactory.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Siv3DMain.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/System/CSystem.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/System/SivSystem_SDL3.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/System/SivSystem_SDL3_Base.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/TextInput/CTextInput.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/TextToSpeech/CTextToSpeech.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Texture/TextureFactory.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Time/SivTime.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/ToastNotification/CToastNotification.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Unicode/SivUnicode_SDL3.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/WebcamInfo/SivWebcamInfo.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Window/CWindow.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/Window/WindowFactory.cpp
  Siv3D/src/Siv3D-Platform/SDL3/Siv3D/XInput/XInputFactory.cpp
)

set(SIV3D_INTERNAL_SOURCES
 
#  Siv3D/src/Siv3D-Platform/OpenGL4/Siv3D/ConstantBuffer/GL4/ConstantBufferDetail_GL4.cpp
#  Siv3D/src/Siv3D-Platform/OpenGL4/Siv3D/Mesh/GL4/CMesh_GL4.cpp
#  Siv3D/src/Siv3D-Platform/OpenGL4/Siv3D/Mesh/GL4/GL4Mesh.cpp
#  Siv3D/src/Siv3D-Platform/OpenGL4/Siv3D/Renderer/GL4/BackBuffer/GL4BackBuffer.cpp
#  Siv3D/src/Siv3D-Platform/OpenGL4/Siv3D/Renderer/GL4/BackBuffer/GL4InternalTexture2D.cpp
#  Siv3D/src/Siv3D-Platform/OpenGL4/Siv3D/Renderer/GL4/BlendState/GL4BlendState.cpp
#  Siv3D/src/Siv3D-Platform/OpenGL4/Siv3D/Renderer/GL4/DepthStencilState/GL4DepthStencilState.cpp
#  Siv3D/src/Siv3D-Platform/OpenGL4/Siv3D/Renderer/GL4/RasterizerState/GL4RasterizerState.cpp
#  Siv3D/src/Siv3D-Platform/OpenGL4/Siv3D/Renderer/GL4/SamplerState/GL4SamplerState.cpp
#  Siv3D/src/Siv3D-Platform/OpenGL4/Siv3D/Renderer2D/GL4/CRenderer2D_GL4.cpp
#  Siv3D/src/Siv3D-Platform/OpenGL4/Siv3D/Renderer2D/GL4/GL4Renderer2DCommand.cpp
#  Siv3D/src/Siv3D-Platform/OpenGL4/Siv3D/Renderer2D/GL4/GL4Vertex2DBatch.cpp
#  Siv3D/src/Siv3D-Platform/OpenGL4/Siv3D/Renderer3D/GL4/CRenderer3D_GL4.cpp
#  Siv3D/src/Siv3D-Platform/OpenGL4/Siv3D/Renderer3D/GL4/GL4Line3DBatch.cpp
#  Siv3D/src/Siv3D-Platform/OpenGL4/Siv3D/Renderer3D/GL4/GL4Renderer3DCommand.cpp
#  Siv3D/src/Siv3D-Platform/OpenGL4/Siv3D/Shader/GL4/CShader_GL4.cpp
#  Siv3D/src/Siv3D-Platform/OpenGL4/Siv3D/Shader/GL4/PixelShader/GL4PixelShader.cpp
#  Siv3D/src/Siv3D-Platform/OpenGL4/Siv3D/Shader/GL4/VertexShader/GL4VertexShader.cpp
#  Siv3D/src/Siv3D-Platform/OpenGL4/Siv3D/Texture/GL4/CTexture_GL4.cpp
#  Siv3D/src/Siv3D-Platform/OpenGL4/Siv3D/Texture/GL4/GL4Texture.cpp

  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/ConstantBuffer/GLES3/ConstantBufferDetail_GLES3.cpp
  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/Mesh/GLES3/CMesh_GLES3.cpp
  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/Mesh/GLES3/GLES3Mesh.cpp
  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/Renderer/GLES3/BackBuffer/GLES3BackBuffer.cpp
  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/Renderer/GLES3/BackBuffer/GLES3InternalTexture2D.cpp
  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/Renderer/GLES3/BlendState/GLES3BlendState.cpp
  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/Renderer/GLES3/DepthStencilState/GLES3DepthStencilState.cpp
  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/Renderer/GLES3/RasterizerState/GLES3RasterizerState.cpp
  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/Renderer/GLES3/SamplerState/GLES3SamplerState.cpp
  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/Renderer2D/GLES3/CRenderer2D_GLES3.cpp
  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/Renderer2D/GLES3/GLES3Renderer2DCommand.cpp
  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/Renderer2D/GLES3/GLES3Vertex2DBatch.cpp
  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/Renderer3D/GLES3/CRenderer3D_GLES3.cpp
  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/Renderer3D/GLES3/GLES3Line3DBatch.cpp
  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/Renderer3D/GLES3/GLES3Renderer3DCommand.cpp
  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/Shader/GLES3/CShader_GLES3.cpp
  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/Shader/GLES3/PixelShader/GLES3PixelShader.cpp
  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/Shader/GLES3/VertexShader/GLES3VertexShader.cpp
  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/Shader/GLES3/ShaderPipeline/GLES3ShaderPipeline.cpp
  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/Texture/GLES3/CTexture_GLES3.cpp
  Siv3D/src/Siv3D-Platform/OpenGLES3/Siv3D/Texture/GLES3/GLES3Texture.cpp

  Siv3D/src/Siv3D/Addon/AddonFactory.cpp
  Siv3D/src/Siv3D/Addon/CAddon.cpp
  Siv3D/src/Siv3D/Addon/SivAddon.cpp
  Siv3D/src/Siv3D/AnimatedGIFReader/AnimatedGIFReaderDetail.cpp
  Siv3D/src/Siv3D/AnimatedGIFReader/SivAnimatedGIFReader.cpp
  Siv3D/src/Siv3D/AnimatedGIFWriter/AnimatedGIFWriterDetail.cpp
  Siv3D/src/Siv3D/AnimatedGIFWriter/SivAnimatedGIFWriter.cpp
  Siv3D/src/Siv3D/ArcEmitter2D/SivArcEmitter2D.cpp
  Siv3D/src/Siv3D/Asset/AssetFactory.cpp
  Siv3D/src/Siv3D/Asset/CAsset.cpp
  Siv3D/src/Siv3D/Asset/IAssetDetail.cpp
  Siv3D/src/Siv3D/Asset/SivAsset.cpp
  Siv3D/src/Siv3D/AssetMonitor/AssetMonitorFactory.cpp
  Siv3D/src/Siv3D/AssetMonitor/CAssetMonitor.cpp
  Siv3D/src/Siv3D/AsyncHTTPTask/AsyncHTTPTaskDetail.cpp
  Siv3D/src/Siv3D/AsyncHTTPTask/SivAsyncHTTPTask.cpp
  Siv3D/src/Siv3D/Audio/AudioBus.cpp
  Siv3D/src/Siv3D/Audio/AudioData.cpp
  Siv3D/src/Siv3D/Audio/AudioFactory.cpp
  Siv3D/src/Siv3D/Audio/CAudio.cpp
  Siv3D/src/Siv3D/Audio/DynamicAudioSource.cpp
  Siv3D/src/Siv3D/Audio/SivAudio.cpp
  Siv3D/src/Siv3D/AudioAsset/SivAudioAsset.cpp
  Siv3D/src/Siv3D/AudioAssetData/SivAudioAssetData.cpp
  Siv3D/src/Siv3D/AudioDecoder/AudioDecoderFactory.cpp
  Siv3D/src/Siv3D/AudioDecoder/CAudioDecoder.cpp
  Siv3D/src/Siv3D/AudioDecoder/SivAudioDecoder.cpp
  Siv3D/src/Siv3D/AudioEncoder/AudioEncoderFactory.cpp
  Siv3D/src/Siv3D/AudioEncoder/CAudioEncoder.cpp
  Siv3D/src/Siv3D/AudioEncoder/SivAudioEncoder.cpp
  Siv3D/src/Siv3D/AudioFormat/MIDI/MIDIDecoder.cpp
  Siv3D/src/Siv3D/AudioFormat/OggVorbis/OggVorbisDecoder.cpp
  Siv3D/src/Siv3D/AudioFormat/OggVorbis/OggVorbisEncoder.cpp
  Siv3D/src/Siv3D/AudioFormat/Opus/OpusDecoder.cpp
  Siv3D/src/Siv3D/AudioFormat/Opus/OpusEncoder.cpp
  Siv3D/src/Siv3D/AudioFormat/WAVE/WAVEDecoder.cpp
  Siv3D/src/Siv3D/AudioFormat/WAVE/WAVEEncoder.cpp
  Siv3D/src/Siv3D/AudioGroup/AudioGroupDetail.cpp
  Siv3D/src/Siv3D/AudioGroup/SivAudioGroup.cpp
  Siv3D/src/Siv3D/Base64/SivBase64.cpp
  Siv3D/src/Siv3D/BasicCamera3D/SivBasicCamera3D.cpp
  Siv3D/src/Siv3D/Bezier2/SivBezier2.cpp
  Siv3D/src/Siv3D/Bezier3/SivBezier3.cpp
  Siv3D/src/Siv3D/BigFloat/SivBigFloat.cpp
  Siv3D/src/Siv3D/BigInt/SivBigInt.cpp
  Siv3D/src/Siv3D/BinaryReader/SivBinaryReader.cpp
  Siv3D/src/Siv3D/BinaryWriter/SivBinaryWriter.cpp
  Siv3D/src/Siv3D/Blob/SivBlob.cpp
  Siv3D/src/Siv3D/Box/SivBox.cpp
  Siv3D/src/Siv3D/Buffer2D/SivBuffer2D.cpp
  Siv3D/src/Siv3D/Byte/SivByte.cpp
  Siv3D/src/Siv3D/CacheDirectory/CacheDirectory.cpp
  Siv3D/src/Siv3D/Camera2DParameters/SivCamera2DParameters.cpp
  Siv3D/src/Siv3D/Camera2D/SivCamera2D.cpp
  Siv3D/src/Siv3D/CascadeClassifier/CascadeClassifierDetail.cpp
  Siv3D/src/Siv3D/CascadeClassifier/SivCascadeClassifier.cpp
  Siv3D/src/Siv3D/ChildProcess/SivChildProcess.cpp
  Siv3D/src/Siv3D/Circle/SivCircle.cpp
  Siv3D/src/Siv3D/CircleEmitter2D/SivCircleEmitter2D.cpp
  Siv3D/src/Siv3D/Clipboard/ClipboardFactory.cpp
  Siv3D/src/Siv3D/Clipboard/SivClipboard.cpp
  Siv3D/src/Siv3D/Color/SivColor.cpp
  Siv3D/src/Siv3D/Colormap/SivColormap.cpp
  Siv3D/src/Siv3D/CommandLine/SivCommandLine.cpp
  Siv3D/src/Siv3D/Common/Siv3DEngine.cpp
  Siv3D/src/Siv3D/Compression/SivCompression.cpp
  Siv3D/src/Siv3D/Cone/SivCone.cpp
  Siv3D/src/Siv3D/Console/ConsoleFactory.cpp
  Siv3D/src/Siv3D/Console/SivConsole.cpp
  Siv3D/src/Siv3D/ConstantBuffer/Null/ConstantBufferDetail_Null.cpp
  Siv3D/src/Siv3D/ConstantBuffer/SivConstantBuffer.cpp
  Siv3D/src/Siv3D/CPUInfo/SivCPUInfo.cpp
  Siv3D/src/Siv3D/CSV/SivCSV.cpp
  Siv3D/src/Siv3D/Cursor/CCursor_Null.cpp
  Siv3D/src/Siv3D/Cursor/CursorFactory.cpp
  Siv3D/src/Siv3D/Cursor/SivCursor.cpp
  Siv3D/src/Siv3D/Cylinder/SivCylinder.cpp
  Siv3D/src/Siv3D/DateTime/SivDateTime.cpp
  Siv3D/src/Siv3D/DebugCamera3D/SivDebugCamera3D.cpp
  Siv3D/src/Siv3D/Demangle/SivDemangle.cpp
  Siv3D/src/Siv3D/Dialog/SivDialog.cpp
  Siv3D/src/Siv3D/DirectoryWatcher/SivDirectoryWatcher.cpp
  Siv3D/src/Siv3D/Disc/SivDisc.cpp
  Siv3D/src/Siv3D/DragDrop/DragDropFactory.cpp
  Siv3D/src/Siv3D/DragDrop/SivDragDrop.cpp
  Siv3D/src/Siv3D/DrawableText/SivDrawableText.cpp
  Siv3D/src/Siv3D/Duration/SivDuration.cpp
  Siv3D/src/Siv3D/DynamicMesh/SivDynamicMesh.cpp
  Siv3D/src/Siv3D/DynamicTexture/SivDynamicTexture.cpp
  Siv3D/src/Siv3D/Effect/CEffect.cpp
  Siv3D/src/Siv3D/Effect/EffectData.cpp
  Siv3D/src/Siv3D/Effect/EffectFactory.cpp
  Siv3D/src/Siv3D/Effect/SivEffect.cpp
  Siv3D/src/Siv3D/Ellipse/SivEllipse.cpp
  Siv3D/src/Siv3D/Emoji/SivEmoji.cpp
  Siv3D/src/Siv3D/Empty/CEmpty.cpp
  Siv3D/src/Siv3D/Empty/EmptyFactory.cpp
  Siv3D/src/Siv3D/EngineLog/SivEngineLog.cpp
  Siv3D/src/Siv3D/EngineOptions/SivEngineOptions.cpp
  Siv3D/src/Siv3D/Error/SivError.cpp
  Siv3D/src/Siv3D/Exif/SivExif.cpp
  Siv3D/src/Siv3D/FFT/CFFT.cpp
  Siv3D/src/Siv3D/FFT/FFTFactory.cpp
  Siv3D/src/Siv3D/FFT/SivFFT.cpp
  Siv3D/src/Siv3D/FileFilter/SivFileFilter.cpp
  Siv3D/src/Siv3D/FileSystem/SivFileSystem.cpp
  Siv3D/src/Siv3D/Font/CFont.cpp
  Siv3D/src/Siv3D/Font/CFont_Headless.cpp
  Siv3D/src/Siv3D/Font/EmojiData.cpp
  Siv3D/src/Siv3D/Font/FontCommon.cpp
  Siv3D/src/Siv3D/Font/GlyphCache/BitmapGlyphCache.cpp
  Siv3D/src/Siv3D/Font/GlyphCache/GlyphCacheCommon.cpp
  Siv3D/src/Siv3D/Font/GlyphCache/MSDFGlyphCache.cpp
  Siv3D/src/Siv3D/Font/GlyphCache/SDFGlyphCache.cpp
  Siv3D/src/Siv3D/Font/GlyphRenderer/agg/agg.cpp
  Siv3D/src/Siv3D/Font/GlyphRenderer/GlyphRenderer.cpp
  Siv3D/src/Siv3D/Font/GlyphRenderer/BitmapGlyphRenderer.cpp
  Siv3D/src/Siv3D/Font/GlyphRenderer/MSDFGlyphRenderer.cpp
  Siv3D/src/Siv3D/Font/GlyphRenderer/OutlineGlyphRenderer.cpp
  Siv3D/src/Siv3D/Font/GlyphRenderer/SDFGlyphRenderer.cpp
  Siv3D/src/Siv3D/Font/FontData.cpp
  Siv3D/src/Siv3D/Font/FontFace.cpp
  Siv3D/src/Siv3D/Font/FontFactory.cpp
  Siv3D/src/Siv3D/Font/IconData.cpp
  Siv3D/src/Siv3D/Font/SivFont.cpp
  Siv3D/src/Siv3D/FontAsset/SivFontAsset.cpp
  Siv3D/src/Siv3D/FontAssetData/SivFontAssetData.cpp
  Siv3D/src/Siv3D/FormatData/SivFormatData.cpp
  Siv3D/src/Siv3D/FormatFloat/SivFormatFloat.cpp
  Siv3D/src/Siv3D/FormatInt/SivFormatInt.cpp
  Siv3D/src/Siv3D/Formatter/SivFormatter.cpp
  Siv3D/src/Siv3D/FormatUtility/SivFormatUtility.cpp
  Siv3D/src/Siv3D/Gamepad/GamepadFactory.cpp
  Siv3D/src/Siv3D/Gamepad/SivGamepad.cpp
  Siv3D/src/Siv3D/GamepadInfo/SivGamepadInfo.cpp
  Siv3D/src/Siv3D/GeoJSON/SivGeoJSON.cpp
  Siv3D/src/Siv3D/Geometry2D/SivGeometry2D.cpp
  Siv3D/src/Siv3D/Geometry3D/SivGeometry3D.cpp
  Siv3D/src/Siv3D/GlobalAudio/SivGlobalAudio.cpp
  Siv3D/src/Siv3D/GrabCut/GrabCutDetail.cpp
  Siv3D/src/Siv3D/GrabCut/SivGrabCut.cpp
  Siv3D/src/Siv3D/Graphics/SivGraphics.cpp
  Siv3D/src/Siv3D/Graphics2D/SivGraphics2D.cpp
  Siv3D/src/Siv3D/Graphics3D/SivGraphics3D.cpp
  Siv3D/src/Siv3D/GUI/CGUI.cpp
  Siv3D/src/Siv3D/GUI/GUIFactory.cpp
  Siv3D/src/Siv3D/Hash/SivHash.cpp
  Siv3D/src/Siv3D/HSV/SivHSV.cpp
  Siv3D/src/Siv3D/HTMLWriter/HTMLWriterDetail.cpp
  Siv3D/src/Siv3D/HTMLWriter/SivHTMLWriter.cpp
  Siv3D/src/Siv3D/HTTPResponse/SivHTTPResponse.cpp
  Siv3D/src/Siv3D/Icon/SivIcon.cpp
  Siv3D/src/Siv3D/Image/ImagePainting.cpp
  Siv3D/src/Siv3D/Image/ShapePainting.cpp
  Siv3D/src/Siv3D/Image/SivImage.cpp
  Siv3D/src/Siv3D/ImageDecoder/CImageDecoder.cpp
  Siv3D/src/Siv3D/ImageDecoder/ImageDecoderFactory.cpp
  Siv3D/src/Siv3D/ImageDecoder/SivImageDecoder.cpp
  Siv3D/src/Siv3D/ImageEncoder/CImageEncoder.cpp
  Siv3D/src/Siv3D/ImageEncoder/ImageEncoderFactory.cpp
  Siv3D/src/Siv3D/ImageEncoder/SivImageEncoder.cpp 
  Siv3D/src/Siv3D/ImageFormat/BMP/BMPDecoder.cpp
  Siv3D/src/Siv3D/ImageFormat/BMP/BMPEncoder.cpp
  Siv3D/src/Siv3D/ImageFormat/GIF/GIFDecoder.cpp
  Siv3D/src/Siv3D/ImageFormat/GIF/GIFEncoder.cpp
  Siv3D/src/Siv3D/ImageFormat/JPEG/JPEGDecoder.cpp
  Siv3D/src/Siv3D/ImageFormat/JPEG/JPEGEncoder.cpp
  Siv3D/src/Siv3D/ImageFormat/PNG/PNGDecoder.cpp
  Siv3D/src/Siv3D/ImageFormat/PNG/PNGEncoder.cpp
  Siv3D/src/Siv3D/ImageFormat/PPM/PPMDecoder.cpp
  Siv3D/src/Siv3D/ImageFormat/PPM/PPMEncoder.cpp
  Siv3D/src/Siv3D/ImageFormat/SVG/SVGDecoder.cpp
  Siv3D/src/Siv3D/ImageFormat/TGA/TGADecoder.cpp
  Siv3D/src/Siv3D/ImageFormat/TGA/TGAEncoder.cpp
  Siv3D/src/Siv3D/ImageFormat/TIFF/TIFFDecoder.cpp
  Siv3D/src/Siv3D/ImageFormat/WebP/WebPDecoder.cpp
  Siv3D/src/Siv3D/ImageFormat/WebP/WebPEncoder.cpp
  Siv3D/src/Siv3D/ImageProcessing/SivImageProcessing.cpp
  Siv3D/src/Siv3D/ImageROI/SivImageROI.cpp
  Siv3D/src/Siv3D/InfinitePlane/SivInfinitePlane.cpp
  Siv3D/src/Siv3D/INI/SivINI.cpp
  Siv3D/src/Siv3D/Input/InputState.cpp
  Siv3D/src/Siv3D/Input/SivInput.cpp
  Siv3D/src/Siv3D/InputCombination/SivInputCombination.cpp
  Siv3D/src/Siv3D/InputGroup/SivInputGroup.cpp
  Siv3D/src/Siv3D/Int128/SivInt128.cpp
  Siv3D/src/Siv3D/Interpolation/SivInterpolation.cpp
  Siv3D/src/Siv3D/IntFormatter/SivIntFormatter.cpp
  Siv3D/src/Siv3D/IPv4Address/SivIPv4Address.cpp
  Siv3D/src/Siv3D/JoyCon/SivJoyCon.cpp
  Siv3D/src/Siv3D/JSON/SivJSON.cpp
  Siv3D/src/Siv3D/Keyboard/KeyboardFactory.cpp
  Siv3D/src/Siv3D/Keyboard/SivKeyboard.cpp
  Siv3D/src/Siv3D/KlattTTS/SivKlattTTS.cpp
  Siv3D/src/Siv3D/LicenseManager/CLicenseManager.cpp
  Siv3D/src/Siv3D/LicenseManager/LicenseManagerFactory.cpp
  Siv3D/src/Siv3D/LicenseManager/SivLicenseManager.cpp
  Siv3D/src/Siv3D/Line/SivLine.cpp
  Siv3D/src/Siv3D/Line3D/SivLine3D.cpp
  Siv3D/src/Siv3D/LineString/SivLineString.cpp
  Siv3D/src/Siv3D/Logger/LoggerFactory.cpp
  Siv3D/src/Siv3D/Logger/SivLogger.cpp
  Siv3D/src/Siv3D/ManagedScript/ManagedScriptDetail.cpp
  Siv3D/src/Siv3D/ManagedScript/SivManagedScript.cpp
  Siv3D/src/Siv3D/Mat3x2/SivMat3x2.cpp
  Siv3D/src/Siv3D/Mat3x3/SivMat3x3.cpp
  Siv3D/src/Siv3D/Mat4x4/SivMat4x4.cpp
  Siv3D/src/Siv3D/MathParser/MathParserDetail.cpp
  Siv3D/src/Siv3D/MathParser/SivMathParser.cpp
  Siv3D/src/Siv3D/MD5/SivMD5.cpp
  Siv3D/src/Siv3D/MD5Value/SivMD5Value.cpp
  Siv3D/src/Siv3D/MemoryMappedFileView/SivMemoryMappedFileView.cpp
  Siv3D/src/Siv3D/MemoryMappedFile/SivMemoryMappedFile.cpp
  Siv3D/src/Siv3D/MemoryReader/SivMemoryReader.cpp
  Siv3D/src/Siv3D/MemoryViewReader/SivMemoryViewReader.cpp
  Siv3D/src/Siv3D/MemoryWriter/SivMemoryWriter.cpp
  Siv3D/src/Siv3D/Mesh/Null/CMesh_Null.cpp
  Siv3D/src/Siv3D/Mesh/SivMesh.cpp
  Siv3D/src/Siv3D/MeshData/MeshUtility.cpp
  Siv3D/src/Siv3D/MeshData/SivMeshData.cpp
  Siv3D/src/Siv3D/MessageBox/SivMessageBox.cpp
  Siv3D/src/Siv3D/Microphone/MicrophoneDetail.cpp
  Siv3D/src/Siv3D/Microphone/SivMicrophone.cpp
  Siv3D/src/Siv3D/MicrophoneInfo/SivMicrophoneInfo.cpp
  Siv3D/src/Siv3D/MicrosecClock/SivMicrosecClock.cpp
  Siv3D/src/Siv3D/MillisecClock/SivMillisecClock.cpp
  Siv3D/src/Siv3D/Model/CModel.cpp
  Siv3D/src/Siv3D/Model/ModelData.cpp
  Siv3D/src/Siv3D/Model/ModelFactory.cpp
  Siv3D/src/Siv3D/Model/SivModel.cpp
  Siv3D/src/Siv3D/ModelObject/SivModelObject.cpp
  Siv3D/src/Siv3D/Monitor/SivMonitor.cpp
  Siv3D/src/Siv3D/Morton/SivMorton.cpp
  Siv3D/src/Siv3D/Mouse/MouseFactory.cpp
  Siv3D/src/Siv3D/Mouse/SivMouse.cpp
  Siv3D/src/Siv3D/MSRenderTexture/SivMSRenderTexture.cpp
  Siv3D/src/Siv3D/MultiPolygon/SivMultiPolygon.cpp
  Siv3D/src/Siv3D/NavMesh/NavMeshDetail.cpp
  Siv3D/src/Siv3D/NavMesh/SivNavMesh.cpp
  Siv3D/src/Siv3D/Network/CNetwork.cpp
  Siv3D/src/Siv3D/Network/NetworkFactory.cpp
  Siv3D/src/Siv3D/Network/SivNetwork.cpp
  Siv3D/src/Siv3D/NinePatch/NinePatchDetail.cpp
  Siv3D/src/Siv3D/NinePatch/SivNinePatch.cpp
  Siv3D/src/Siv3D/None/SivNone.cpp
  Siv3D/src/Siv3D/OpenAI/OpenAICommon.cpp
  Siv3D/src/Siv3D/OpenAI/SivOpenAIChat.cpp
  Siv3D/src/Siv3D/OpenAI/SivOpenAIEmbedding.cpp
  Siv3D/src/Siv3D/OpenAI/SivOpenAIImage.cpp
  Siv3D/src/Siv3D/OpenAI/SivOpenAISpeech.cpp
  Siv3D/src/Siv3D/OpenAI/SivOpenAIVision.cpp
  Siv3D/src/Siv3D/OpenCV_Bridge/SivOpenCV_Bridge.cpp
  Siv3D/src/Siv3D/OrientedBox/SivOrientedBox.cpp
  Siv3D/src/Siv3D/OSCMessage/OSCMessageDetail.cpp
  Siv3D/src/Siv3D/OSCMessage/SivOSCMessage.cpp
  Siv3D/src/Siv3D/OSCReceiver/OSCPacketListener.cpp
  Siv3D/src/Siv3D/OSCReceiver/OSCReceiverDetail.cpp
  Siv3D/src/Siv3D/OSCReceiver/SivOSCReceiver.cpp
  Siv3D/src/Siv3D/OSCSender/SivOSCSender.cpp
  Siv3D/src/Siv3D/Parse/SivParse.cpp
  Siv3D/src/Siv3D/ParseBool/SivParseBool.cpp
  Siv3D/src/Siv3D/ParseFloat/SivParseFloat.cpp
  Siv3D/src/Siv3D/ParseInt/SivParseInt.cpp
  Siv3D/src/Siv3D/Particle2D/SivParticle2D.cpp
  Siv3D/src/Siv3D/ParticleSystem2D/ParticleSystem2DDetail.cpp
  Siv3D/src/Siv3D/ParticleSystem2D/SivParticleSystem2D.cpp
  Siv3D/src/Siv3D/Pentablet/Null/CPentablet_Null.cpp
  Siv3D/src/Siv3D/Pentablet/SivPentablet.cpp
  Siv3D/src/Siv3D/Periodic/SivPeriodic.cpp
  Siv3D/src/Siv3D/Physics2D/P2Body.cpp
  Siv3D/src/Siv3D/Physics2D/P2BodyDetail.cpp
  Siv3D/src/Siv3D/Physics2D/P2Circle.cpp
  Siv3D/src/Siv3D/Physics2D/P2ContactListener.cpp
  Siv3D/src/Siv3D/Physics2D/P2DistanceJoint.cpp
  Siv3D/src/Siv3D/Physics2D/P2DistanceJointDetail.cpp
  Siv3D/src/Siv3D/Physics2D/P2Line.cpp
  Siv3D/src/Siv3D/Physics2D/P2LineString.cpp
  Siv3D/src/Siv3D/Physics2D/P2MouseJoint.cpp
  Siv3D/src/Siv3D/Physics2D/P2MouseJointDetail.cpp
  Siv3D/src/Siv3D/Physics2D/P2PivotJoint.cpp
  Siv3D/src/Siv3D/Physics2D/P2PivotJointDetail.cpp
  Siv3D/src/Siv3D/Physics2D/P2Polygon.cpp
  Siv3D/src/Siv3D/Physics2D/P2Quad.cpp
  Siv3D/src/Siv3D/Physics2D/P2Rect.cpp
  Siv3D/src/Siv3D/Physics2D/P2Shape.cpp
  Siv3D/src/Siv3D/Physics2D/P2SliderJoint.cpp
  Siv3D/src/Siv3D/Physics2D/P2SliderJointDetail.cpp
  Siv3D/src/Siv3D/Physics2D/P2Triangle.cpp
  Siv3D/src/Siv3D/Physics2D/P2WheelJoint.cpp
  Siv3D/src/Siv3D/Physics2D/P2WheelJointDetail.cpp
  Siv3D/src/Siv3D/Physics2D/P2World.cpp
  Siv3D/src/Siv3D/Physics2D/P2WorldDetail.cpp
  Siv3D/src/Siv3D/PixelShader/SivPixelShader.cpp
  Siv3D/src/Siv3D/PixelShaderAsset/SivPixelShaderAsset.cpp
  Siv3D/src/Siv3D/PixelShaderAssetData/SivPixelShaderAssetData.cpp
  Siv3D/src/Siv3D/Plane/SivPlane.cpp
  Siv3D/src/Siv3D/PlayingCard/SivPlayingCard.cpp
  Siv3D/src/Siv3D/Point3D/SivPoint3D.cpp
  Siv3D/src/Siv3D/Point/SivPoint.cpp
  Siv3D/src/Siv3D/Polygon/PolygonDetail.cpp
  Siv3D/src/Siv3D/Polygon/SivPolygon.cpp
  Siv3D/src/Siv3D/Polygon/Triangulation.cpp
  Siv3D/src/Siv3D/PolygonEmitter2D/SivPolygonEmitter2D.cpp
  Siv3D/src/Siv3D/PrimeNumber/SivPrimeNumber.cpp
  Siv3D/src/Siv3D/PrimitiveMesh/CPrimitiveMesh.cpp
  Siv3D/src/Siv3D/PrimitiveMesh/PrimitiveMeshFactory.cpp
  Siv3D/src/Siv3D/Print/CPrint.cpp
  Siv3D/src/Siv3D/Print/PrintFactory.cpp
  Siv3D/src/Siv3D/Print/SivPrint.cpp
  Siv3D/src/Siv3D/ProController/SivProController.cpp
  Siv3D/src/Siv3D/Profiler/CProfiler.cpp
  Siv3D/src/Siv3D/Profiler/ProfilerFactory.cpp
  Siv3D/src/Siv3D/Profiler/SivProfiler.cpp
  Siv3D/src/Siv3D/ProfilerStat/SivProfilerStat.cpp
  Siv3D/src/Siv3D/PutText/SivPutText.cpp
  Siv3D/src/Siv3D/QR/SivQR.cpp
  Siv3D/src/Siv3D/QRScanner/QRScannerDetail.cpp
  Siv3D/src/Siv3D/QRScanner/SivQRScanner.cpp
  Siv3D/src/Siv3D/Quad/SivQuad.cpp
  Siv3D/src/Siv3D/Quaternion/SivQuaternion.cpp
  Siv3D/src/Siv3D/Random/SivRandom.cpp
  Siv3D/src/Siv3D/Ray/SivRay.cpp
  Siv3D/src/Siv3D/RDTSCClock/SivRDTSCClock.cpp
  Siv3D/src/Siv3D/Rect/SivRect.cpp
  Siv3D/src/Siv3D/RectanglePacking/SivRectanglePacking.cpp
  Siv3D/src/Siv3D/RectEmitter2D/SivRectEmitter2D.cpp
  Siv3D/src/Siv3D/RectF/SivRectF.cpp
  Siv3D/src/Siv3D/RegExp/RegExpDetail.cpp
  Siv3D/src/Siv3D/RegExp/SivRegExp.cpp
  Siv3D/src/Siv3D/Renderer/Null/CRenderer_Null.cpp
  Siv3D/src/Siv3D/Renderer2D/Null/CRenderer2D_Null.cpp
  Siv3D/src/Siv3D/Renderer2D/Vertex2DBuilder.cpp
  Siv3D/src/Siv3D/Renderer3D/Null/CRenderer3D_Null.cpp
  Siv3D/src/Siv3D/RenderTexture/SivRenderTexture.cpp
  Siv3D/src/Siv3D/Resource/ResourceFactory.cpp
  Siv3D/src/Siv3D/Resource/SivResource.cpp
  Siv3D/src/Siv3D/RoundRect/SivRoundRect.cpp
  Siv3D/src/Siv3D/Say/SivSay.cpp
  Siv3D/src/Siv3D/Scene/CScene.cpp
  Siv3D/src/Siv3D/Scene/FrameCounter.cpp
  Siv3D/src/Siv3D/Scene/FrameTimer.cpp
  Siv3D/src/Siv3D/Scene/SceneFactory.cpp
  Siv3D/src/Siv3D/Scene/SivScene.cpp
  Siv3D/src/Siv3D/ScopedColorAdd2D/SivScopedColorAdd2D.cpp
  Siv3D/src/Siv3D/ScopedColorMul2D/SivScopedColorMul2D.cpp
  Siv3D/src/Siv3D/ScopedCustomShader2D/SivScopedCustomShader2D.cpp
  Siv3D/src/Siv3D/ScopedCustomShader3D/SivScopedCustomShader3D.cpp
  Siv3D/src/Siv3D/ScopedRenderStates2D/SivScopedRenderStates2D.cpp
  Siv3D/src/Siv3D/ScopedRenderStates3D/SivScopedRenderStates3D.cpp
  Siv3D/src/Siv3D/ScopedRenderTarget2D/SivScopedRenderTarget2D.cpp
  Siv3D/src/Siv3D/ScopedRenderTarget3D/SivScopedRenderTarget3D.cpp
  Siv3D/src/Siv3D/ScopedViewport2D/SivScopedViewport2D.cpp
  Siv3D/src/Siv3D/ScopedViewport3D/SivScopedViewport3D.cpp
  Siv3D/src/Siv3D/ScreenCapture/CScreenCapture.cpp
  Siv3D/src/Siv3D/ScreenCapture/ScreenCaptureFactory.cpp
  Siv3D/src/Siv3D/ScreenCapture/SivScreenCapture.cpp
  Siv3D/src/Siv3D/ScriptFunction/SivScriptFunction.cpp
  Siv3D/src/Siv3D/ScriptModule/SivScriptModule.cpp
  Siv3D/src/Siv3D/Script/angelscript/scriptarray.cpp
  Siv3D/src/Siv3D/Script/angelscript/scriptbuilder.cpp
  Siv3D/src/Siv3D/Script/angelscript/scriptgrid.cpp
  Siv3D/src/Siv3D/Script/angelscript/scriptstdstring.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptAudio.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptAudioFormat.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptBezier2.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptBezier3.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptCamera2D.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptCamera2DParameters.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptCameraControl.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptChar.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptCircle.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptCircular.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptColor.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptColorF.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptCopyOption.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptCursor.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptCursorStyle.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptDate.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptDateTime.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptDialog.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptDrawableText.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptDuration.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptDynamicTexture.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptEasing.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptEllipse.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptEmoji.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptFileSystem.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptFloat2.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptFloat3.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptFloat4.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptFloatingPoint.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptFloatRect.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptFont.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptFontMethod.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptFontStyle.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptFormat.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptGlobalAudio.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptGMInstrument.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptGraphics.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptGraphics2D.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptHSV.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptIcon.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptImage.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptImageFormat.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptINI.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptInput.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptInterpolation.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptKeyboard.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptLanguageCode.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptLicenseInfo.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptLicenseManager.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptLine.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptLineString.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptLineStyle.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptLineStyleParameters.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptMat3x2.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptMath.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptMathConstants.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptMicrosecClock.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptMillisecClock.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptMixBus.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptMonitor.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptMouse.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptNamedParameter.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptNone_t.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptNumber.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptOffsetCircular.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptOpenMode.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptOptional.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptPalette.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptParse.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptPeriodic.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptPianoKey.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptPoint.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptPolygon.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptPrimeNumber.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptPrint.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptProfiler.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptPutText.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptQuad.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptRandom.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptRDTSCClock.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptRect.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptRectF.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptResizeMode.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptResource.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptRoundRect.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptSay.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptScene.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptScopedViewport2D.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptShape2D.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptSimpleGUI.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptSpecialFolder.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptSpline2D.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptStopwatch.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptSystem.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTextEditState.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTextEncoding.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTextInput.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTextInputMode.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTextReader.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTextStyle.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTextToSpeech.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTexture.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTexturedCircle.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTextureDesc.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTexturedQuad.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTexturedRoundRect.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTextureFormat.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTexturePixelFormat.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTextureRegion.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTextWriter.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptThreading.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTime.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTimer.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTransformer2D.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTriangle.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTriangleIndex.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTypeface.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptTypes.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptUserAction.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptUtility.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptVariableSpeedStopwatch.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptVec2.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptVec3.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptVec4.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptVideoTexture.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptWave.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptWaveSample.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptWindow.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptWindowStyle.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptXInput.cpp
  Siv3D/src/Siv3D/Script/Bind/ScriptYesNo.cpp
  Siv3D/src/Siv3D/Script/CScript.cpp
  Siv3D/src/Siv3D/Script/ScriptData.cpp
  Siv3D/src/Siv3D/Script/ScriptFactory.cpp
  Siv3D/src/Siv3D/Script/SivScript.cpp
  Siv3D/src/Siv3D/SerialPortInfo/SivSerialPortInfo.cpp
  Siv3D/src/Siv3D/Serial/SerialDetail.cpp
  Siv3D/src/Siv3D/Serial/SivSerial.cpp
  Siv3D/src/Siv3D/SFMT/SivSFMT.cpp
  Siv3D/src/Siv3D/ShaderCommon/SivShaderCommon.cpp
  Siv3D/src/Siv3D/Shader/Null/CShader_Null.cpp
  Siv3D/src/Siv3D/Shader/SivShader.cpp
  Siv3D/src/Siv3D/Shape2D/SivShape2D.cpp
  Siv3D/src/Siv3D/SIMDMath/SivSIMDMath.cpp
  Siv3D/src/Siv3D/SimpleAnimation/SivSimpleAnimation.cpp
  Siv3D/src/Siv3D/SimpleFollowCamera3D/SivSimpleFollowCamera3D.cpp
  Siv3D/src/Siv3D/SimpleGUI/SivSimpleGUI.cpp
  Siv3D/src/Siv3D/SimpleHTTP/SivSimpleHTTP.cpp
  Siv3D/src/Siv3D/SimpleMenuBar/SivSimpleMenuBar.cpp
  Siv3D/src/Siv3D/SimpleTable/SivSimpleTable.cpp
  Siv3D/src/Siv3D/Sky/SivSky.cpp
  Siv3D/src/Siv3D/SoundFont/CSoundFont.cpp
  Siv3D/src/Siv3D/SoundFont/SivSoundFont.cpp
  Siv3D/src/Siv3D/SoundFont/SoundFontDetail.cpp
  Siv3D/src/Siv3D/SoundFont/SoundFontFactory.cpp
  Siv3D/src/Siv3D/Sphere/SivSphere.cpp
  Siv3D/src/Siv3D/Spline2D/SivSpline2D.cpp
  Siv3D/src/Siv3D/String/SivString.cpp
  Siv3D/src/Siv3D/String/Levenshtein.cpp
  Siv3D/src/Siv3D/StringView/SivStringView.cpp
  Siv3D/src/Siv3D/Subdivision2D/SivSubdivision2D.cpp
  Siv3D/src/Siv3D/SVG/SivSVG.cpp
  Siv3D/src/Siv3D/SVG/SVGDetail.cpp
  Siv3D/src/Siv3D/System/SivSystem.cpp
  Siv3D/src/Siv3D/System/SystemFactory.cpp
  Siv3D/src/Siv3D/System/SystemLog.cpp
  Siv3D/src/Siv3D/System/SystemMisc.cpp
  Siv3D/src/Siv3D/TCPClient/SivTCPClient.cpp
  Siv3D/src/Siv3D/TCPClient/TCPClientDetail.cpp
  Siv3D/src/Siv3D/TCPServer/SivTCPServer.cpp
  Siv3D/src/Siv3D/TCPServer/TCPServerDetail.cpp
  Siv3D/src/Siv3D/TextAreaEditState/SivTextAreaEditState.cpp
  Siv3D/src/Siv3D/TextEncoding/SivTextEncoding.cpp
  Siv3D/src/Siv3D/TextInput/SivTextInput.cpp
  Siv3D/src/Siv3D/TextInput/TextInputFactory.cpp
  Siv3D/src/Siv3D/TextReader/SivTextReader.cpp
  Siv3D/src/Siv3D/TextReader/TextReaderDetail.cpp
  Siv3D/src/Siv3D/TextToSpeech/SivTextToSpeech.cpp
  Siv3D/src/Siv3D/TextToSpeech/TextToSpeechFactory.cpp
  Siv3D/src/Siv3D/Texture/Null/CTexture_Null.cpp
  Siv3D/src/Siv3D/Texture/SivTexture.cpp
  Siv3D/src/Siv3D/Texture/TextureCommon.cpp
  Siv3D/src/Siv3D/TextureAsset/SivTextureAsset.cpp
  Siv3D/src/Siv3D/TextureAssetData/SivTextureAssetData.cpp
  Siv3D/src/Siv3D/TexturedCircle/SivTexturedCircle.cpp
  Siv3D/src/Siv3D/TexturedQuad/SivTexturedQuad.cpp
  Siv3D/src/Siv3D/TexturedRoundRect/SivTexturedRoundRect.cpp
  Siv3D/src/Siv3D/TextureFormat/SivTextureFormat.cpp
  Siv3D/src/Siv3D/TextureRegion/SivTextureRegion.cpp
  Siv3D/src/Siv3D/TextWriter/SivTextWriter.cpp
  Siv3D/src/Siv3D/TextWriter/TextWriterDetail.cpp  
  Siv3D/src/Siv3D/Threading/SivThreading.cpp
  Siv3D/src/Siv3D/TimeProfiler/SivTimeProfiler.cpp
  Siv3D/src/Siv3D/Timer/SivTimer.cpp
  Siv3D/src/Siv3D/ToastNotification/SivToastNotification.cpp
  Siv3D/src/Siv3D/ToastNotification/ToastNotificationFactory.cpp
  Siv3D/src/Siv3D/TOMLReader/SivTOMLReader.cpp
  Siv3D/src/Siv3D/Triangle/SivTriangle.cpp
  Siv3D/src/Siv3D/TrailMotion/SivTrailMotion.cpp
  Siv3D/src/Siv3D/TrailRenderer/CTrailRenderer.cpp
  Siv3D/src/Siv3D/TrailRenderer/TrailRendererFactory.cpp
  Siv3D/src/Siv3D/Trail/SivTrail.cpp
  Siv3D/src/Siv3D/Transformer2D/SivTransformer2D.cpp
  Siv3D/src/Siv3D/Transformer3D/SivTransformer3D.cpp
  Siv3D/src/Siv3D/Triangle3D/SivTriangle3D.cpp
  Siv3D/src/Siv3D/Troubleshooting/Troubleshooting.cpp
  Siv3D/src/Siv3D/Twitter/SivTwitter.cpp
  Siv3D/src/Siv3D/Unicode/SivUnicode.cpp
  Siv3D/src/Siv3D/Unicode/UnicodeUtility.cpp
  Siv3D/src/Siv3D/UnicodeConverter/SivUnicodeConverter.cpp
  Siv3D/src/Siv3D/UserAction/CUserAction.cpp
  Siv3D/src/Siv3D/UserAction/UserActionFactory.cpp
  Siv3D/src/Siv3D/UUIDValue/SivUUIDValue.cpp
  Siv3D/src/Siv3D/VariableSpeedStopwatch/SivVariableSpeedStopwatch.cpp
  Siv3D/src/Siv3D/Vector2D/SivVector2D.cpp
  Siv3D/src/Siv3D/Vector3D/SivVector3D.cpp
  Siv3D/src/Siv3D/Vector4D/SivVector4D.cpp
  Siv3D/src/Siv3D/VertexShader/SivVertexShader.cpp
  Siv3D/src/Siv3D/VertexShaderAsset/SivVertexShaderAsset.cpp
  Siv3D/src/Siv3D/VertexShaderAssetData/SivVertexShaderAssetData.cpp
  Siv3D/src/Siv3D/VideoReader/SivVideoReader.cpp
  Siv3D/src/Siv3D/VideoReader/VideoReaderDetail.cpp
  Siv3D/src/Siv3D/VideoWriter/SivVideoWriter.cpp
  Siv3D/src/Siv3D/VideoWriter/VideoWriterDetail.cpp
  Siv3D/src/Siv3D/VideoTexture/SivVideoTexture.cpp
  Siv3D/src/Siv3D/VideoTexture/VideoTextureDetail.cpp
  Siv3D/src/Siv3D/ViewFrustum/SivViewFrustum.cpp
  Siv3D/src/Siv3D/Wave/SivWave.cpp
  Siv3D/src/Siv3D/Webcam/SivWebcam.cpp
  Siv3D/src/Siv3D/Webcam/WebcamDetail.cpp
  Siv3D/src/Siv3D/Window/SivWindow.cpp
  Siv3D/src/Siv3D/Window/Null/CWindow_Null.cpp
  Siv3D/src/Siv3D/XInput/Null/CXInput_Null.cpp
  Siv3D/src/Siv3D/XInput/SivXInput.cpp
  Siv3D/src/Siv3D/XMLReader/SivXMLReader.cpp
  Siv3D/src/Siv3D/ZIPReader/SivZIPReader.cpp
  Siv3D/src/Siv3D/ZIPReader/ZIPReaderDetail.cpp
  Siv3D/src/Siv3D/Zlib/SivZlib.cpp
)

# ---------------------------------------------
# ThirdParty libraries
# ---------------------------------------------
add_subdirectory(Siv3D/src/ThirdParty/absl)
add_subdirectory(Siv3D/src/ThirdParty/angelscript)
add_subdirectory(Siv3D/src/ThirdParty/box2d)
add_subdirectory(Siv3D/src/ThirdParty/cpu_features)
add_subdirectory(Siv3D/src/ThirdParty/DirectXMath)
add_subdirectory(Siv3D/src/ThirdParty/double-conversion)
add_subdirectory(Siv3D/src/ThirdParty/easyexif)
add_subdirectory(Siv3D/src/ThirdParty/fmt)
#add_subdirectory(Siv3D/src/ThirdParty/GLEW)
add_subdirectory(Siv3D/src/ThirdParty/glad)
add_subdirectory(Siv3D/src/ThirdParty/infoware)
add_subdirectory(Siv3D/src/ThirdParty/lunasvg)
add_subdirectory(Siv3D/src/ThirdParty/minizip)
add_subdirectory(Siv3D/src/ThirdParty/msdfgen)
add_subdirectory(Siv3D/src/ThirdParty/muparser)
add_subdirectory(Siv3D/src/ThirdParty/nativefiledialog-extended)
add_subdirectory(Siv3D/src/ThirdParty/nlohmann)
add_subdirectory(Siv3D/src/ThirdParty/Oniguruma)
add_subdirectory(Siv3D/src/ThirdParty/oscpack)
add_subdirectory(Siv3D/src/ThirdParty/pffft)
add_subdirectory(Siv3D/src/ThirdParty/plutovg)
add_subdirectory(Siv3D/src/ThirdParty/qr-code-generator-library)
add_subdirectory(Siv3D/src/ThirdParty/quirc)
add_subdirectory(Siv3D/src/ThirdParty/RecastDetour)
add_subdirectory(Siv3D/src/ThirdParty/rtaudio)
add_subdirectory(Siv3D/src/ThirdParty/serial)
add_subdirectory(Siv3D/src/ThirdParty/skia)
add_subdirectory(Siv3D/src/ThirdParty/soloud)
add_subdirectory(Siv3D/src/ThirdParty/SplineLib)
add_subdirectory(Siv3D/src/ThirdParty/tinyobjloader)
add_subdirectory(Siv3D/src/ThirdParty/tinyxml)
add_subdirectory(Siv3D/src/ThirdParty/zstd)

set(SIV3D_INTERNAL_INCLUDE_DIRS
  Siv3D/include
  Siv3D/include/ThirdParty
  Siv3D/src
  ${Boost_INCLUDE_DIRS}
  Siv3D/src/Siv3D-Platform/SDL3
  Siv3D/src/Siv3D-Platform/OpenGLES3
  Siv3D/src/ThirdParty
  Siv3D/src/ThirdParty/asio
  Siv3D/src/ThirdParty/glad/include
  ${SIV3D_THIRD_PARTY_INCLUDE_DIRS}
)


add_library(Siv3D STATIC
  ${SIV3D_INTERNAL_SOURCES}
  # ThirdParty OBJECT libraries
  $<TARGET_OBJECTS:Siv3D_ThirdParty_absl>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_angelscript>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_box2d>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_cpu_features>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_DirectXMath>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_double_conversion>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_easyexif>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_fmt>
#  $<TARGET_OBJECTS:Siv3D_ThirdParty_GLEW>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_GLAD>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_infoware>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_lunasvg>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_minizip>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_msdfgen>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_muparser>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_nfd>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_nlohmann>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_Oniguruma>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_oscpack>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_pffft>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_plutovg>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_qrcode>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_quirc>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_RecastDetour>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_rtaudio>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_serial>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_skia>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_soloud>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_SplineLib>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_tinyobjloader>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_tinyxml>
  $<TARGET_OBJECTS:Siv3D_ThirdParty_zstd>
  ${SIV3D_SDL3_SOURCES}
  # giflib quantize.c (removed from giflib 5.2.2+, needed by GIFEncoder)
  Siv3D/src/ThirdParty/giflib/quantize.c
)
target_link_libraries(Siv3D INTERFACE
  ${CMAKE_THREAD_LIBS_INIT}
  ${GIF_LIBRARIES}  
  ZLIB::ZLIB
  PNG::PNG
  JPEG::JPEG
  TIFF::TIFF
  Opus::opus
	OpusFile::opusfile
  Vorbis::vorbis
  Vorbis::vorbisenc
  Vorbis::vorbisfile
  WebP::webp
  CURL::libcurl
  Freetype::Freetype
  harfbuzz::harfbuzz
  GIF::GIF
#  unofficial::mpg123::libmpg123
#  SoundTouch::SoundTouch
  ${OpenCV_LIBS}
  ${CMAKE_DL_LIBS}
  $<$<PLATFORM_ID:Windows>:version>
  $<$<PLATFORM_ID:Windows>:winmm>
  $<$<TARGET_EXISTS:libjpeg-turbo::turbojpeg-static>:libjpeg-turbo::turbojpeg-static>
)
target_link_libraries(Siv3D
PUBLIC
  SDL3::SDL3
)
if(UNIX AND NOT APPLE)
  target_link_libraries(Siv3D PRIVATE PkgConfig::SIV3D_LINUX_GTK)
  target_link_libraries(Siv3D_ThirdParty_nfd PRIVATE PkgConfig::SIV3D_LINUX_GTK)
  # stduuid (UUIDValue::Generate) は Linux では libuuid の uuid_generate を使う。
  # 実行時の依存を増やさないよう静的ライブラリを優先する
  find_library(SIV3D_LINUX_UUID_LIBRARY NAMES libuuid.a uuid REQUIRED)
  target_link_libraries(Siv3D INTERFACE ${SIV3D_LINUX_UUID_LIBRARY})
  if(CMAKE_SYSTEM_PROCESSOR MATCHES "x86_64|AMD64")
    # Platform.hpp は Linux では __SSE4_2__ があるときだけネイティブの SSE 組み込み関数を
    # 使い、無いと simde の互換定義 (SIMDE_ENABLE_NATIVE_ALIASES) に倒れる。gcc-14 では
    # その互換定義が <immintrin.h> 系と衝突してコンパイルできないため SSE4.2 を有効にする
    # (Windows / macOS は常にネイティブ経路)。SIMD.hpp を経由して利用側にも要るので PUBLIC
    target_compile_options(Siv3D PUBLIC -msse4.2)
  endif()
endif()
target_compile_features(Siv3D PRIVATE cxx_std_20 c_std_11)
target_include_directories(Siv3D
  PRIVATE
    $<BUILD_INTERFACE: ${SIV3D_INTERNAL_INCLUDE_DIRS}>
    $<BUILD_INTERFACE: ${SIV3D_THIRD_PARTY_INCLUDE_DIRS}>
  INTERFACE
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Siv3D/include>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/Siv3D/include/ThirdParty>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/Siv3D/ThirdParty>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}/Siv3D>
)
set_target_properties(Siv3D PROPERTIES
  C_EXTENSIONS OFF
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
target_compile_options(Siv3D PRIVATE
  $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/bigobj>
  $<$<COMPILE_LANGUAGE:CXX>: ${SIV3D_CXX_FLAGS} ${SIV3D_CXX_FLAGS_WITH_COMPILER}
    $<$<CONFIG:Debug>:   ${SIV3D_CXX_FLAGS_DEBUG}>
    $<$<CONFIG:Release>: ${SIV3D_CXX_FLAGS_RELEASE}>
    $<$<CONFIG:RelWithDebInfo>: ${SIV3D_CXX_FLAGS_RELWITHDEBINFO}>
    $<$<CONFIG:MinSizeRel>: ${SIV3D_CXX_FLAGS_MINSIZEREL}>
  >
  $<$<COMPILE_LANGUAGE:C>: ${SIV3D_C_FLAGS} ${SIV3D_CXX_FLAGS_WITH_COMPILER}
    $<$<CONFIG:Debug>:   ${SIV3D_C_FLAGS_DEBUG}>
    $<$<CONFIG:Release>: ${SIV3D_C_FLAGS_RELEASE}>
    $<$<CONFIG:RelWithDebInfo>: ${SIV3D_C_FLAGS_RELWITHDEBINFO}>
    $<$<CONFIG:MinSizeRel>: ${SIV3D_C_FLAGS_MINSIZEREL}>
  >
)
target_compile_definitions(Siv3D PRIVATE
  $<$<COMPILE_LANGUAGE:CXX>: ${SIV3D_CXX_DEFINITION}>
  $<$<COMPILE_LANGUAGE:C>:   ${SIV3D_C_DEFINITION}>
  ONIG_STATIC
)
add_library(Siv3D::Siv3D ALIAS Siv3D)

# Siv3D install settings
message(STATUS "[info] CMAKE_INSTALL_LIBDIR: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}")
message(STATUS "[info] CMAKE_INSTALL_INCLUDEDIR: ${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_INCLUDEDIR}/Siv3D")

# SDL3の関連ターゲットをリストに追加
set(SDL3_INSTALL_TARGETS "")
if(TARGET SDL3-shared)
  list(APPEND SDL3_INSTALL_TARGETS SDL3-shared)
endif()
if(TARGET SDL3-static)
  list(APPEND SDL3_INSTALL_TARGETS SDL3-static)
endif()
if(TARGET SDL3_Headers)
  list(APPEND SDL3_INSTALL_TARGETS SDL3_Headers)
endif()

install(
  TARGETS Siv3D ${SDL3_INSTALL_TARGETS}
  EXPORT Siv3DTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
install(
  EXPORT Siv3DTargets
  FILE Siv3DConfig.cmake
  NAMESPACE Siv3D::
  EXPORT_LINK_INTERFACE_LIBRARIES
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/Siv3D
)
install(
  FILES Siv3D/include/Siv3D.hpp Siv3D/include/HamFramework.hpp
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Siv3D
)
install(
  DIRECTORY Siv3D/include/Siv3D Siv3D/include/ThirdParty
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/Siv3D
)

# ---------------------------------------------
# Sample App
# ---------------------------------------------

option(SIV3D_BUILD_SAMPLE "Build Siv3D sample application" OFF)

if(SIV3D_BUILD_SAMPLE)
  add_executable(Siv3DApp SDL3/App/Main.cpp)
  target_link_libraries(Siv3DApp PRIVATE Siv3D::Siv3D)
  target_compile_features(Siv3DApp PRIVATE cxx_std_20)
  set_target_properties(Siv3DApp PROPERTIES
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/SDL3/App"
  )
  target_compile_options(Siv3DApp PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/utf-8>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/bigobj>
  )
  target_compile_definitions(Siv3DApp PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:_UNICODE>
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
    SIV3D_USE_SDL3
  )
  # Copy SDL3 DLL to the executable output directory
  add_custom_command(TARGET Siv3DApp POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
      $<TARGET_FILE:SDL3::SDL3-shared>
      $<TARGET_FILE_DIR:Siv3DApp>
    COMMENT "Copying SDL3 DLL to output directory"
  )

  if(UNIX AND NOT APPLE)
    # Linux: steamrt sniper SDK (glibc 2.31) でのビルド・配布を想定した設定 (SDL3/BUILD_LINUX.md)
    #
    # 同梱 libSDL3 を exe と同じフォルダから確実に読ませる。Steam 起動時は
    # LD_LIBRARY_PATH の先頭に Steam ランタイムや /usr/lib が入るため、
    # LD_LIBRARY_PATH の後に探される RUNPATH ではシステム側の libSDL3 が勝ってしまう。
    # LD_LIBRARY_PATH より先に探される DT_RPATH ($ORIGIN) にする。
    set_target_properties(Siv3DApp PROPERTIES
      BUILD_RPATH "\$ORIGIN"
      INSTALL_RPATH "\$ORIGIN"
    )
    target_link_options(Siv3DApp PRIVATE "LINKER:--disable-new-dtags")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      # 例外の巻き戻し処理 (libgcc の unwinder) は共有の libgcc_s.so.1 から解決させる。
      # sniper SDK の gcc-14 は libstdc++ / libgcc を静的にリンクする (-shared-libgcc も
      # 効かない) ため、共有ライブラリ側と unwinder の複製を持ち合わないよう直接リンクする。
      # libstdc++ は静的のままなので GLIBCXX 依存は増えない。
      target_link_options(Siv3DApp PRIVATE
        "SHELL:-Wl,--push-state,--no-as-needed -l:libgcc_s.so.1 -Wl,--pop-state")
    endif()
    # exe が NEEDED で引く soname (libSDL3.so.0) のリンクもビルドツリーに置く
    add_custom_command(TARGET Siv3DApp POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E create_symlink
        $<TARGET_FILE_NAME:SDL3::SDL3-shared>
        $<TARGET_FILE_DIR:Siv3DApp>/$<TARGET_SONAME_FILE_NAME:SDL3::SDL3-shared>
    )

    # install: <prefix>/ 直下に そのまま起動できる形で配置する
    #   Siv3DApp, libSDL3.so.0.x.y, libSDL3.so.0, resources/ (エンジン資材), example/
    install(TARGETS Siv3DApp RUNTIME DESTINATION .)
    install(FILES
      $<TARGET_FILE:SDL3::SDL3-shared>
      $<TARGET_SONAME_FILE:SDL3::SDL3-shared>
      DESTINATION .)
  endif()
endif()
