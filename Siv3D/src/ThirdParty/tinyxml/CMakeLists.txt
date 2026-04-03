# tinyxml library
add_library(Siv3D_ThirdParty_tinyxml OBJECT
  tinyxml2.cpp
)
target_include_directories(Siv3D_ThirdParty_tinyxml PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(Siv3D_ThirdParty_tinyxml PRIVATE cxx_std_20)
set_target_properties(Siv3D_ThirdParty_tinyxml PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
