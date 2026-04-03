# double-conversion library
add_library(Siv3D_ThirdParty_double_conversion OBJECT
  bignum-dtoa.cc
  bignum.cc
  cached-powers.cc
  double-to-string.cc
  fast-dtoa.cc
  fixed-dtoa.cc
)
target_include_directories(Siv3D_ThirdParty_double_conversion PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(Siv3D_ThirdParty_double_conversion PRIVATE cxx_std_20)
set_target_properties(Siv3D_ThirdParty_double_conversion PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
