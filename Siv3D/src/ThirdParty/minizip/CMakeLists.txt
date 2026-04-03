# minizip library

# Platform-specific sources
if(WIN32)
  set(MINIZIP_PLATFORM_SOURCES
    mz_os_win32.c
    mz_strm_os_win32.c
  )
else()
  set(MINIZIP_PLATFORM_SOURCES
    mz_os_posix.c
    mz_strm_os_posix.c
  )
endif()

add_library(Siv3D_ThirdParty_minizip OBJECT
  mz_compat.c
  mz_crypt.c
  mz_crypt_brg.c
  mz_os.c
  mz_strm.c
  mz_strm_buf.c
  mz_strm_mem.c
  mz_strm_pkcrypt.c
  mz_strm_split.c
  mz_strm_wzaes.c
  mz_strm_zlib.c
  mz_zip.c
  mz_zip_rw.c
  ${MINIZIP_PLATFORM_SOURCES}
)
target_include_directories(Siv3D_ThirdParty_minizip PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(Siv3D_ThirdParty_minizip PRIVATE c_std_11)
# ZLIB is linked via Siv3D target
set_target_properties(Siv3D_ThirdParty_minizip PROPERTIES
  C_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
