# DirectXMath library
add_library(Siv3D_ThirdParty_DirectXMath OBJECT
  DirectXCollision.cpp
)
target_include_directories(Siv3D_ThirdParty_DirectXMath PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
target_compile_features(Siv3D_ThirdParty_DirectXMath PRIVATE cxx_std_20)
set_target_properties(Siv3D_ThirdParty_DirectXMath PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
