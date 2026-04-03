# GLEW library
add_library(Siv3D_ThirdParty_GLEW OBJECT
  glew.c
)
target_include_directories(Siv3D_ThirdParty_GLEW PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(Siv3D_ThirdParty_GLEW PRIVATE c_std_11)
set_target_properties(Siv3D_ThirdParty_GLEW PROPERTIES
  C_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
