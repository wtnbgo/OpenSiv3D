# absl library
add_library(Siv3D_ThirdParty_absl OBJECT
  numeric/int128.cc
  random/discrete_distribution.cc
  random/gaussian_distribution.cc
)
target_include_directories(Siv3D_ThirdParty_absl PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/ThirdParty
)

target_compile_features(Siv3D_ThirdParty_absl PRIVATE cxx_std_20)
set_target_properties(Siv3D_ThirdParty_absl PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
