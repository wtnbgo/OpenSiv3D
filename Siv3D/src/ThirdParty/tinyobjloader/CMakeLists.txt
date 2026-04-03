# tinyobjloader library
add_library(Siv3D_ThirdParty_tinyobjloader OBJECT
  tiny_obj_loader.cc
)
target_include_directories(Siv3D_ThirdParty_tinyobjloader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(Siv3D_ThirdParty_tinyobjloader PRIVATE cxx_std_20)
set_target_properties(Siv3D_ThirdParty_tinyobjloader PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
