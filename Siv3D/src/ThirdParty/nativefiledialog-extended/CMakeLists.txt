# nativefiledialog-extended library

# Platform-specific sources
if(WIN32)
  set(NFD_PLATFORM_SOURCES nfd_win.cpp)
  set(NFD_COMPILE_FEATURES cxx_std_20)
elseif(APPLE)
  set(NFD_PLATFORM_SOURCES nfd_cocoa.m)
  # Objective-C doesn't need cxx_std_20
  set(NFD_COMPILE_FEATURES "")
else()
  set(NFD_PLATFORM_SOURCES nfd_gtk.cpp)
  set(NFD_COMPILE_FEATURES cxx_std_20)
endif()

add_library(Siv3D_ThirdParty_nfd OBJECT
  ${NFD_PLATFORM_SOURCES}
)
target_include_directories(Siv3D_ThirdParty_nfd PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
if(NFD_COMPILE_FEATURES)
  target_compile_features(Siv3D_ThirdParty_nfd PRIVATE ${NFD_COMPILE_FEATURES})
endif()
set_target_properties(Siv3D_ThirdParty_nfd PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
