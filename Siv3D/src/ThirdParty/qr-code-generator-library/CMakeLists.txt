# qr-code-generator-library
add_library(Siv3D_ThirdParty_qrcode OBJECT
  qrcodegen.cpp
)
target_include_directories(Siv3D_ThirdParty_qrcode PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(Siv3D_ThirdParty_qrcode PRIVATE cxx_std_20)
set_target_properties(Siv3D_ThirdParty_qrcode PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
