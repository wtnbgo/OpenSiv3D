# -----------------------------------------------------------------------------
# siv3d_add_app(): Siv3D (SDL3 版) のアプリの実行ファイルを作る
#
#   siv3d_add_app(<target>
#     SOURCES <file>...                # アプリのソース
#     [CONFIG <siv3d-app.json>]        # パッケージ定義 (表示名・実行ファイル名・バージョン・バンドル ID・アセット・機種別設定)
#     [ASSETS <from>[|<to>]...]        # 実行ファイルの隣にコピーするフォルダ (CONFIG の "assets" に追加)
#     [OUTPUT_DIRECTORY <dir>]         # 実行ファイルの出力先 (既定: CMake の既定)
#   )
#
# やること (全機種共通):
#   - Siv3D へのリンク、コンパイル設定
#   - ビルド後に、SDL3 の共有ライブラリ・エンジンのリソース (resources/engine)・アセットを実行ファイルの隣にコピー
#     (ビルドツリーからそのまま起動できる)
#   - install() で配布用の構成を作る (cmake --install <build> --prefix <dist>)
# 機種別:
#   - Windows: コンソールの有無 (windows.console)、アイコンとバージョン情報 (icon / windows.icon)
#   - Linux  : RPATH = $ORIGIN (DT_RPATH)、libgcc_s の直接リンク、同梱 SDL3 / FFmpeg とライセンス文
#   - macOS  : .app バンドル、ANGLE の dylib を Contents/Frameworks に同梱、アイコン (macos.icon)
#
# OpenSiv3D のトップレベル CMakeLists.txt から include される (SIV3D_SOURCE_DIR が設定済み)。
# -----------------------------------------------------------------------------

include_guard(GLOBAL)

# JSON の値を取り出す (無ければ既定値)
function(_siv3d_json_get out json default)
  string(JSON _value ERROR_VARIABLE _error GET "${json}" ${ARGN})
  if(_error OR (_value STREQUAL ""))
    set(${out} "${default}" PARENT_SCOPE)
  else()
    set(${out} "${_value}" PARENT_SCOPE)
  endif()
endfunction()

function(siv3d_add_app target)
  cmake_parse_arguments(PARSE_ARGV 1 ARG "" "CONFIG;OUTPUT_DIRECTORY" "SOURCES;ASSETS")

  if(NOT SIV3D_SOURCE_DIR)
    message(FATAL_ERROR "siv3d_add_app: SIV3D_SOURCE_DIR is not set (include OpenSiv3D first)")
  endif()

  # ---------------------------------------------------------------------------
  # パッケージ定義 (siv3d-app.json)
  # ---------------------------------------------------------------------------
  set(_json "{}")
  set(_config_dir "${CMAKE_CURRENT_SOURCE_DIR}")
  if(ARG_CONFIG)
    get_filename_component(_config_path "${ARG_CONFIG}" ABSOLUTE)
    get_filename_component(_config_dir "${_config_path}" DIRECTORY)
    file(READ "${_config_path}" _json)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_config_path}")
  endif()

  _siv3d_json_get(_name "${_json}" "${target}" name)
  _siv3d_json_get(_exe_name "${_json}" "${target}" exeName)
  _siv3d_json_get(_version "${_json}" "1.0.0" version)
  _siv3d_json_get(_bundle_id "${_json}" "com.example.${target}" bundleId)
  _siv3d_json_get(_company "${_json}" "" company)
  _siv3d_json_get(_copyright "${_json}" "" copyright)
  _siv3d_json_get(_windows_console "${_json}" "OFF" windows console)
  _siv3d_json_get(_windows_icon "${_json}" "" windows icon)
  _siv3d_json_get(_macos_icon "${_json}" "" macos icon)

  # アセット: [{ "from": "assets", "to": "assets" }, ...] (to を省略すると from と同じ名前)
  set(_assets "")
  string(JSON _asset_count ERROR_VARIABLE _error LENGTH "${_json}" assets)
  if(NOT _error AND _asset_count GREATER 0)
    math(EXPR _last "${_asset_count} - 1")
    foreach(i RANGE ${_last})
      _siv3d_json_get(_from "${_json}" "" assets ${i} from)
      _siv3d_json_get(_to "${_json}" "${_from}" assets ${i} to)
      list(APPEND _assets "${_from}|${_to}")
    endforeach()
  endif()
  list(APPEND _assets ${ARG_ASSETS})

  # バージョン番号 (Windows のバージョン情報用に 4 要素へ)
  string(REGEX MATCHALL "[0-9]+" _version_parts "${_version}")
  list(APPEND _version_parts 0 0 0 0)
  list(SUBLIST _version_parts 0 4 _version_parts)
  string(JOIN "," _version_comma ${_version_parts})

  # ---------------------------------------------------------------------------
  # 実行ファイル
  # ---------------------------------------------------------------------------
  if(ANDROID)
    # Android: SDL の Java 側 (SDLActivity) が読み込む共有ライブラリ libmain.so にする
    add_library(${target} SHARED ${ARG_SOURCES})
  else()
    add_executable(${target} ${ARG_SOURCES})
  endif()
  target_link_libraries(${target} PRIVATE Siv3D::Siv3D)
  target_compile_features(${target} PRIVATE cxx_std_20)
  set_target_properties(${target} PROPERTIES
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
    OUTPUT_NAME "${_exe_name}"
  )
  if(ARG_OUTPUT_DIRECTORY)
    set_target_properties(${target} PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${ARG_OUTPUT_DIRECTORY}")
  endif()
  target_compile_options(${target} PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/utf-8>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/bigobj>
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<COMPILE_LANGUAGE:CXX>>:/Zc:__cplusplus>
  )
  target_compile_definitions(${target} PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:_UNICODE>
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
    SIV3D_USE_SDL3
  )

  set(_exe_dir "$<TARGET_FILE_DIR:${target}>")

  # ---------------------------------------------------------------------------
  # Android: libmain.so を作るだけ。APK への格納・アセット・署名は外枠 (siv3d_and の Gradle) が行う
  #   エンジンのリソースとアセットは APK の assets に入れ、初回起動時に内部ストレージへ展開して
  #   そのフォルダを Siv3D のアプリディレクトリ (起動引数) として渡す
  # ---------------------------------------------------------------------------
  if(ANDROID)
    set_target_properties(${target} PROPERTIES OUTPUT_NAME "main")
    target_link_libraries(${target} PRIVATE android log)
    # Siv3D (静的ライブラリ) の SDL_main を libmain.so に取り込んで公開する
    target_link_options(${target} PRIVATE "-Wl,--undefined=SDL_main")
    return()
  endif()

  # ---------------------------------------------------------------------------
  # Web (Emscripten): <exeName>.html / .js / .wasm / .data を作る
  #   エンジンのリソースとアセットは .data にまとめて仮想ファイルシステムに置く (--preload-file)。
  #   実行ファイルのパスは /this.program になるので、Resource() は /resources/、アセットは / からの相対パス
  # ---------------------------------------------------------------------------
  if(EMSCRIPTEN)
    _siv3d_json_get(_web_initial_memory "${_json}" "512MB" web initialMemory)
    _siv3d_json_get(_web_maximum_memory "${_json}" "4GB" web maximumMemory)
    _siv3d_json_get(_web_shell "${_json}" "${SIV3D_SOURCE_DIR}/SDL3/Web/shell.html" web shell)
    get_filename_component(_web_shell "${_web_shell}" ABSOLUTE BASE_DIR "${_config_dir}")

    _siv3d_json_get(_web_persist "${_json}" "ON" web persist)

    set_target_properties(${target} PROPERTIES SUFFIX ".html")

    # セーブデータの永続化: ホームフォルダ (/home/<exeName>) を IndexedDB (IDBFS) にする (SDL3/Web/persist.pre.js.in)
    if(_web_persist)
      string(REGEX REPLACE "[^A-Za-z0-9_.-]" "_" _web_home_name "${_exe_name}")
      set(SIV3D_WEB_HOME "/home/${_web_home_name}")
      set(_web_pre_js "${CMAKE_CURRENT_BINARY_DIR}/${target}-persist.pre.js")
      configure_file("${SIV3D_SOURCE_DIR}/SDL3/Web/persist.pre.js.in" "${_web_pre_js}" @ONLY)
      target_link_options(${target} PRIVATE "SHELL:--pre-js \"${_web_pre_js}\"")
      set_property(TARGET ${target} APPEND PROPERTY LINK_DEPENDS "${_web_pre_js}")
    endif()
    target_link_options(${target} PRIVATE
      # while (System::Update()) のループからブラウザに制御を返す (CRenderer_GLES3::present)
      "SHELL:-s ASYNCIFY=1"
      "SHELL:-s ASYNCIFY_STACK_SIZE=1048576"
      # スレッド (AsyncTask など)。メインスレッドはワーカーの起動を待てないので、先に用意しておく
      "SHELL:-s PTHREAD_POOL_SIZE=navigator.hardwareConcurrency+8"
      "SHELL:-s PTHREAD_POOL_SIZE_STRICT=0"
      # メモリ
      "SHELL:-s INITIAL_MEMORY=${_web_initial_memory}"
      "SHELL:-s MAXIMUM_MEMORY=${_web_maximum_memory}"
      "SHELL:-s ALLOW_MEMORY_GROWTH=1"
      "SHELL:-s STACK_SIZE=8MB"
      "SHELL:-s DEFAULT_PTHREAD_STACK_SIZE=2MB"
      # WebGL 2 (= OpenGL ES 3.0)。glad で関数を取り出すので GetProcAddress を有効にする
      "SHELL:-s MIN_WEBGL_VERSION=2"
      "SHELL:-s MAX_WEBGL_VERSION=2"
      "SHELL:-s FULL_ES3=1"
      "SHELL:-s GL_ENABLE_GET_PROC_ADDRESS=1"
      # ファイルシステム (セーブデータ用の IndexedDB 永続化を含む)
      # HTTP (SimpleHTTP / AsyncHTTPTask) は fetch API
      "SHELL:-s FETCH=1"
      "SHELL:-s FORCE_FILESYSTEM=1"
      "-lidbfs.js"
      # miniaudio の Web Audio 実装 (EM_ASM) が Module.HEAPF32 を使う
      "SHELL:-s EXPORTED_RUNTIME_METHODS=ccall,FS,HEAPF32,HEAP32,HEAPU8"
      # SDL3 の JS 側 (EM_ASM) が Module._malloc / _free を使う
      "SHELL:-s EXPORTED_FUNCTIONS=_main,_malloc,_free"
      "SHELL:--shell-file \"${_web_shell}\""
      $<$<CONFIG:Debug>:SHELL:-s ASSERTIONS=1>
    )

    set(_web_outputs
      "${_exe_dir}/${_exe_name}.html"
      "${_exe_dir}/${_exe_name}.js"
      "${_exe_dir}/${_exe_name}.wasm"
      "${_exe_dir}/${_exe_name}.data"
    )

    set(_engine_resource_src "${SIV3D_SOURCE_DIR}/Web/App/resources/engine")
    foreach(_dir font soundfont texture shader/essl)
      target_link_options(${target} PRIVATE "SHELL:--preload-file \"${_engine_resource_src}/${_dir}@/resources/engine/${_dir}\"")
    endforeach()
    foreach(_asset IN LISTS _assets)
      string(REPLACE "|" ";" _pair "${_asset}")
      list(GET _pair 0 _from)
      list(LENGTH _pair _pair_length)
      if(_pair_length GREATER 1)
        list(GET _pair 1 _to)
      else()
        set(_to "${_from}")
      endif()
      get_filename_component(_from_path "${_from}" ABSOLUTE BASE_DIR "${_config_dir}")
      if(IS_DIRECTORY "${_from_path}")
        target_link_options(${target} PRIVATE "SHELL:--preload-file \"${_from_path}@/${_to}\"")
      else()
        message(WARNING "siv3d_add_app(${target}): asset directory not found: ${_from_path}")
      endif()
    endforeach()

    # 配布: 4 ファイルと、確認用の配信スクリプト (COOP/COEP ヘッダ付き)
    install(FILES ${_web_outputs} DESTINATION .)
    install(FILES "${SIV3D_SOURCE_DIR}/SDL3/Web/serve.py" DESTINATION .)
    return()
  endif()

  # ---------------------------------------------------------------------------
  # Windows: コンソール・アイコン・バージョン情報
  # ---------------------------------------------------------------------------
  if(WIN32)
    if(NOT _windows_console)
      # コンソールを出さない (エントリポイントは main のまま)
      set_target_properties(${target} PROPERTIES WIN32_EXECUTABLE ON)
      if(MSVC)
        target_link_options(${target} PRIVATE "/ENTRY:mainCRTStartup")
      endif()
    endif()

    set(_rc "${CMAKE_CURRENT_BINARY_DIR}/${target}_resource.rc")
    set(_rc_icon "")
    if(_windows_icon)
      get_filename_component(_icon_path "${_windows_icon}" ABSOLUTE BASE_DIR "${_config_dir}")
      file(TO_CMAKE_PATH "${_icon_path}" _icon_path)
      set(_rc_icon "1 ICON \"${_icon_path}\"\n")
    endif()
    file(GENERATE OUTPUT "${_rc}" CONTENT
"${_rc_icon}1 VERSIONINFO
FILEVERSION ${_version_comma}
PRODUCTVERSION ${_version_comma}
BEGIN
  BLOCK \"StringFileInfo\"
  BEGIN
    BLOCK \"040904B0\"
    BEGIN
      VALUE \"CompanyName\", \"${_company}\"
      VALUE \"FileDescription\", \"${_name}\"
      VALUE \"FileVersion\", \"${_version}\"
      VALUE \"LegalCopyright\", \"${_copyright}\"
      VALUE \"ProductName\", \"${_name}\"
      VALUE \"ProductVersion\", \"${_version}\"
      VALUE \"OriginalFilename\", \"${_exe_name}.exe\"
    END
  END
  BLOCK \"VarFileInfo\"
  BEGIN
    VALUE \"Translation\", 0x0409, 1200
  END
END
")
    target_sources(${target} PRIVATE "${_rc}")
  endif()

  # ---------------------------------------------------------------------------
  # SDL3 の共有ライブラリ
  # ---------------------------------------------------------------------------
  if(NOT IOS) # iOS は SDL3 を静的リンクする
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3-shared> "${_exe_dir}"
    )
  endif()

  # ---------------------------------------------------------------------------
  # Linux: steamrt sniper SDK (glibc 2.31) でのビルド・配布を想定した設定 (SDL3/BUILD_LINUX.md)
  # ---------------------------------------------------------------------------
  if(SIV3D_LINUX_DESKTOP)
    # 同梱 libSDL3 を exe と同じフォルダから確実に読ませる。Steam 起動時は
    # LD_LIBRARY_PATH の先頭に Steam ランタイムや /usr/lib が入るため、
    # LD_LIBRARY_PATH の後に探される RUNPATH ではシステム側の libSDL3 が勝ってしまう。
    # LD_LIBRARY_PATH より先に探される DT_RPATH ($ORIGIN) にする。
    set_target_properties(${target} PROPERTIES
      BUILD_RPATH "\$ORIGIN"
      INSTALL_RPATH "\$ORIGIN"
    )
    target_link_options(${target} PRIVATE "LINKER:--disable-new-dtags")
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
      # 例外の巻き戻し処理 (libgcc の unwinder) は共有の libgcc_s.so.1 から解決させる。
      # sniper SDK の gcc-14 は libstdc++ / libgcc を静的にリンクする (-shared-libgcc も
      # 効かない) ため、共有ライブラリ側と unwinder の複製を持ち合わないよう直接リンクする。
      # libstdc++ は静的のままなので GLIBCXX 依存は増えない。
      target_link_options(${target} PRIVATE
        "SHELL:-Wl,--push-state,--no-as-needed -l:libgcc_s.so.1 -Wl,--pop-state")
    endif()
    # exe が NEEDED で引く soname (libSDL3.so.0) のリンクもビルドツリーに置く
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E create_symlink
        $<TARGET_FILE_NAME:SDL3::SDL3-shared>
        "${_exe_dir}/$<TARGET_SONAME_FILE_NAME:SDL3::SDL3-shared>"
    )

    # ffmpeg (opencv4 の VideoCapture が使う) は LGPL を守るため共有ライブラリにしている
    # (cmake/vcpkg-triplets/x64-linux.cmake)。libav* / libsw* の .so を実行ファイルの隣に置き、
    # exe の DT_RPATH ($ORIGIN) で解決させる。ライセンス文は Siv3D 本体の configure で生成済み
    if(SIV3D_LINUX_FFMPEG_LIBS)
      foreach(_lib IN LISTS SIV3D_LINUX_FFMPEG_LIBS)
        add_custom_command(TARGET ${target} POST_BUILD
          COMMAND ${CMAKE_COMMAND} -E copy_if_different "${_lib}" "${_exe_dir}"
        )
      endforeach()
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory "${SIV3D_LINUX_LICENSE_DIR}" "${_exe_dir}/licenses"
      )
    endif()
  endif()

  # ---------------------------------------------------------------------------
  # エンジンのリソース (GLES 用 essl シェーダ / フォント / サウンドフォント / テクスチャ) とアセット
  # Resource() は ModulePath の親ディレクトリ + "resources/" を参照する。
  # macOS の .app では $<TARGET_FILE_DIR> は Contents/MacOS になる (起動時のカレントディレクトリもここ)。
  # ---------------------------------------------------------------------------
  # iOS の .app は平らな構成で、直下の "Resources" (大文字小文字を区別しない) は旧形式のバンドルと解釈されて
  # インストールできなくなるので、エンジンのリソースは siv3d_resources/ に置く (SivResource_SDL3.cpp と同じ)
  if(IOS)
    set(_resource_dir_name "siv3d_resources")
  else()
    set(_resource_dir_name "resources")
  endif()
  set(_engine_resource_src "${SIV3D_SOURCE_DIR}/Web/App/resources/engine")
  foreach(_dir font soundfont texture shader/essl)
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory "${_engine_resource_src}/${_dir}" "${_exe_dir}/${_resource_dir_name}/engine/${_dir}"
    )
  endforeach()

  set(_asset_installs "")
  foreach(_asset IN LISTS _assets)
    string(REPLACE "|" ";" _pair "${_asset}")
    list(GET _pair 0 _from)
    list(LENGTH _pair _pair_length)
    if(_pair_length GREATER 1)
      list(GET _pair 1 _to)
    else()
      set(_to "${_from}")
    endif()
    get_filename_component(_from_path "${_from}" ABSOLUTE BASE_DIR "${_config_dir}")
    if(NOT IS_DIRECTORY "${_from_path}")
      message(WARNING "siv3d_add_app(${target}): asset directory not found: ${_from_path}")
      continue()
    endif()
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_directory "${_from_path}" "${_exe_dir}/${_to}"
    )
    list(APPEND _asset_installs "${_from_path}|${_to}")
  endforeach()

  # ---------------------------------------------------------------------------
  # iOS: .app バンドル (平らな構成: 実行ファイル・resources/engine・アセットを .app の直下に置く)
  #   siv3d-app.json の "ios": displayName / version / build / deploymentTarget /
  #   orientations ("landscape" / "portrait" / "all" または UIInterfaceOrientation* の配列) / deviceFamily / team
  # ---------------------------------------------------------------------------
  if(IOS)
    _siv3d_json_get(SIV3D_IOS_DISPLAY_NAME "${_json}" "${_name}" ios displayName)
    _siv3d_json_get(SIV3D_IOS_VERSION "${_json}" "${_version}" ios version)
    _siv3d_json_get(SIV3D_IOS_BUILD "${_json}" "1" ios build)
    _siv3d_json_get(SIV3D_IOS_DEPLOYMENT_TARGET "${_json}" "15.0" ios deploymentTarget)
    _siv3d_json_get(_ios_bundle_id "${_json}" "${_bundle_id}" ios bundleId)
    _siv3d_json_get(_ios_device_family "${_json}" "1,2" ios deviceFamily)
    _siv3d_json_get(_ios_team "${_json}" "" ios team)
    set(SIV3D_IOS_BUNDLE_ID "${_ios_bundle_id}")
    set(SIV3D_IOS_EXECUTABLE "${_exe_name}")

    # 画面の向き
    set(_orientations "")
    string(JSON _orientation_type ERROR_VARIABLE _error TYPE "${_json}" ios orientations)
    if(NOT _error AND _orientation_type STREQUAL "ARRAY")
      string(JSON _count LENGTH "${_json}" ios orientations)
      if(_count GREATER 0)
        math(EXPR _last "${_count} - 1")
        foreach(i RANGE ${_last})
          string(JSON _o GET "${_json}" ios orientations ${i})
          list(APPEND _orientations "${_o}")
        endforeach()
      endif()
    else()
      _siv3d_json_get(_orientation_kind "${_json}" "landscape" ios orientations)
      if(_orientation_kind STREQUAL "portrait")
        set(_orientations UIInterfaceOrientationPortrait)
      elseif(_orientation_kind STREQUAL "all")
        set(_orientations UIInterfaceOrientationPortrait UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight UIInterfaceOrientationPortraitUpsideDown)
      else()
        set(_orientations UIInterfaceOrientationLandscapeLeft UIInterfaceOrientationLandscapeRight)
      endif()
    endif()
    set(SIV3D_IOS_ORIENTATIONS_PLIST "")
    foreach(_o IN LISTS _orientations)
      string(APPEND SIV3D_IOS_ORIENTATIONS_PLIST "\t\t<string>${_o}</string>\n")
    endforeach()

    set(_plist "${CMAKE_CURRENT_BINARY_DIR}/${target}-Info.plist")
    configure_file("${SIV3D_SOURCE_DIR}/cmake/ios/Info.plist.in" "${_plist}" @ONLY)

    set_target_properties(${target} PROPERTIES
      MACOSX_BUNDLE ON
      MACOSX_BUNDLE_INFO_PLIST "${_plist}"
      MACOSX_BUNDLE_GUI_IDENTIFIER "${SIV3D_IOS_BUNDLE_ID}"
      MACOSX_BUNDLE_BUNDLE_NAME "${SIV3D_IOS_DISPLAY_NAME}"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${SIV3D_IOS_VERSION}"
      MACOSX_BUNDLE_BUNDLE_VERSION "${SIV3D_IOS_BUILD}"
      XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${SIV3D_IOS_BUNDLE_ID}"
      XCODE_ATTRIBUTE_IPHONEOS_DEPLOYMENT_TARGET "${SIV3D_IOS_DEPLOYMENT_TARGET}"
      XCODE_ATTRIBUTE_TARGETED_DEVICE_FAMILY "${_ios_device_family}"
      XCODE_ATTRIBUTE_CODE_SIGN_STYLE "Automatic"
      XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY "Apple Development"
      "XCODE_ATTRIBUTE_CODE_SIGN_IDENTITY[sdk=iphonesimulator*]" "-"
      XCODE_ATTRIBUTE_DEVELOPMENT_TEAM "${_ios_team}"
      XCODE_ATTRIBUTE_ENABLE_BITCODE "NO"
      XCODE_ATTRIBUTE_SKIP_INSTALL "NO"
      XCODE_ATTRIBUTE_INSTALL_PATH "$(LOCAL_APPS_DIR)"
      XCODE_GENERATE_SCHEME ON
    )

    # 配布: ビルド後にコピーしたものごと .app を置く (ipa は siv3d_app.py が作る)
    install(TARGETS ${target} BUNDLE DESTINATION .)
    return()
  endif()

  # ---------------------------------------------------------------------------
  # macOS: .app バンドル
  # ---------------------------------------------------------------------------
  if(APPLE)
    set_target_properties(${target} PROPERTIES
      MACOSX_BUNDLE ON
      MACOSX_BUNDLE_GUI_IDENTIFIER "${_bundle_id}"
      MACOSX_BUNDLE_BUNDLE_NAME "${_name}"
      MACOSX_BUNDLE_BUNDLE_VERSION "${_version}"
      MACOSX_BUNDLE_SHORT_VERSION_STRING "${_version}"
      MACOSX_BUNDLE_COPYRIGHT "${_copyright}"
    )
    if(_macos_icon)
      get_filename_component(_icon_path "${_macos_icon}" ABSOLUTE BASE_DIR "${_config_dir}")
      get_filename_component(_icon_name "${_icon_path}" NAME)
      set_source_files_properties("${_icon_path}" PROPERTIES MACOSX_PACKAGE_LOCATION "Resources")
      target_sources(${target} PRIVATE "${_icon_path}")
      set_target_properties(${target} PROPERTIES MACOSX_BUNDLE_ICON_FILE "${_icon_name}")
    endif()

    # macOS には OpenGL ES が無いので ANGLE (Metal バックエンド) を使う。
    # SDL3 は GLES コンテキスト要求時に libEGL / libGLESv2 を実行時に dlopen するため、
    # vcpkg の angle (overlay triplet で dylib 化) を Contents/Frameworks に同梱し、
    # パスはエンジン (CWindow::init) が SDL のヒントで渡す。
    # find_package の IMPORTED ターゲットはディレクトリスコープなので、OpenSiv3D (add_subdirectory) 側で
    # 見つけたものはアプリのディレクトリからは見えない。見えなければここで改めて探す
    if(NOT TARGET unofficial::angle::libEGL)
      find_package(unofficial-angle CONFIG REQUIRED)
    endif()
    set(_angle_libs unofficial::angle::libEGL unofficial::angle::libGLESv2)
    if(TARGET unofficial::angle::libANGLE)
      get_target_property(_angle_type unofficial::angle::libANGLE TYPE)
      if(_angle_type STREQUAL "SHARED_LIBRARY")
        list(APPEND _angle_libs unofficial::angle::libANGLE)
      endif()
    endif()
    set(_fw_dir "$<TARGET_BUNDLE_CONTENT_DIR:${target}>/Frameworks")
    add_custom_command(TARGET ${target} POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E make_directory "${_fw_dir}")
    foreach(_lib ${_angle_libs})
      add_custom_command(TARGET ${target} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${_lib}>" "${_fw_dir}/")
    endforeach()
    # libSDL3 (Contents/MacOS) と libEGL → libGLESv2 の依存 (@rpath/...) を解決させる
    set_property(TARGET ${target} APPEND PROPERTY BUILD_RPATH "@executable_path" "@executable_path/../Frameworks")
    set_property(TARGET ${target} APPEND PROPERTY INSTALL_RPATH "@executable_path" "@executable_path/../Frameworks")

    # 配布: ビルド後にコピーしたものごと .app を置く
    install(TARGETS ${target} BUNDLE DESTINATION .)
  else()
    # -------------------------------------------------------------------------
    # 配布 (Windows / Linux): <prefix>/ 直下にそのまま起動できる形で置く
    #   <exe>, SDL3 (.dll / .so と soname のリンク), resources/engine, アセット, (Linux) FFmpeg とライセンス文
    # -------------------------------------------------------------------------
    install(TARGETS ${target} RUNTIME DESTINATION .)
    if(WIN32)
      install(FILES $<TARGET_FILE:SDL3::SDL3-shared> DESTINATION .)
    else()
      install(FILES $<TARGET_FILE:SDL3::SDL3-shared> $<TARGET_SONAME_FILE:SDL3::SDL3-shared> DESTINATION .)
      if(SIV3D_LINUX_FFMPEG_LIBS)
        install(FILES ${SIV3D_LINUX_FFMPEG_LIBS} DESTINATION .)
        install(DIRECTORY "${SIV3D_LINUX_LICENSE_DIR}/" DESTINATION licenses)
      endif()
    endif()
    foreach(_dir font soundfont texture shader/essl)
      install(DIRECTORY "${_engine_resource_src}/${_dir}/" DESTINATION "resources/engine/${_dir}")
    endforeach()
    foreach(_asset IN LISTS _asset_installs)
      string(REPLACE "|" ";" _pair "${_asset}")
      list(GET _pair 0 _from_path)
      list(GET _pair 1 _to)
      install(DIRECTORY "${_from_path}/" DESTINATION "${_to}")
    endforeach()
  endif()
endfunction()
