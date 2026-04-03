# fmt library
add_library(Siv3D_ThirdParty_fmt OBJECT
  format.cc
)
target_include_directories(Siv3D_ThirdParty_fmt PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/ThirdParty
)
target_compile_features(Siv3D_ThirdParty_fmt PRIVATE cxx_std_20)
set_target_properties(Siv3D_ThirdParty_fmt PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
