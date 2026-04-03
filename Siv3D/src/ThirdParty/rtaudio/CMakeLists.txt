# rtaudio library
add_library(Siv3D_ThirdParty_rtaudio OBJECT
  RtAudio.cpp
)
target_include_directories(Siv3D_ThirdParty_rtaudio PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(Siv3D_ThirdParty_rtaudio PRIVATE cxx_std_20)
set_target_properties(Siv3D_ThirdParty_rtaudio PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
