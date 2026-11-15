# Web 版 (Emscripten) の triplet。community triplet (wasm32-emscripten) に -pthread / -fwasm-exceptions を
# 足したもの (cmake/toolchains/wasm32-emscripten.cmake を chainload する)
set(VCPKG_ENV_PASSTHROUGH_UNTRACKED EMSDK EMSCRIPTEN_ROOT PATH)

set(VCPKG_TARGET_ARCHITECTURE wasm32)
set(VCPKG_CRT_LINKAGE dynamic)
set(VCPKG_LIBRARY_LINKAGE static)
set(VCPKG_CMAKE_SYSTEM_NAME Emscripten)
set(VCPKG_CHAINLOAD_TOOLCHAIN_FILE "${CMAKE_CURRENT_LIST_DIR}/../toolchains/wasm32-emscripten.cmake")

# opencv4: Emscripten も UNIX 扱いになり、カメラ (V4L) が既定で有効になって configure の検証で失敗するので切る
if(PORT STREQUAL "opencv4")
  set(VCPKG_CMAKE_CONFIGURE_OPTIONS -DWITH_V4L=OFF)
endif()
