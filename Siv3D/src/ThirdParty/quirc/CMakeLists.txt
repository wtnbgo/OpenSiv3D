# quirc library
add_library(Siv3D_ThirdParty_quirc OBJECT
  decode.c
  identify.c
  quirc.c
  version_db.c
)
target_include_directories(Siv3D_ThirdParty_quirc PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(Siv3D_ThirdParty_quirc PRIVATE c_std_11)
set_target_properties(Siv3D_ThirdParty_quirc PROPERTIES
  C_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
