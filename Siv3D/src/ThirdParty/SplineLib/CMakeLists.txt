# SplineLib library
add_library(Siv3D_ThirdParty_SplineLib OBJECT
  Splines.cpp
)
target_include_directories(Siv3D_ThirdParty_SplineLib PUBLIC 
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/ThirdParty
)
target_compile_features(Siv3D_ThirdParty_SplineLib PRIVATE cxx_std_20)
set_target_properties(Siv3D_ThirdParty_SplineLib PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
