# GLAD library
add_library(Siv3D_ThirdParty_GLAD OBJECT
  src/egl.c
  src/gles2.c
)
target_include_directories(Siv3D_ThirdParty_GLAD PUBLIC include)
target_compile_features(Siv3D_ThirdParty_GLAD PRIVATE c_std_11)
set_target_properties(Siv3D_ThirdParty_GLAD PROPERTIES
  C_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
