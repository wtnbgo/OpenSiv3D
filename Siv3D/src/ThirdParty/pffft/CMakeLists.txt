# pffft library
add_library(Siv3D_ThirdParty_pffft OBJECT
  pffft.c
)
target_include_directories(Siv3D_ThirdParty_pffft PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(Siv3D_ThirdParty_pffft PRIVATE c_std_11)
set_target_properties(Siv3D_ThirdParty_pffft PROPERTIES
  C_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
