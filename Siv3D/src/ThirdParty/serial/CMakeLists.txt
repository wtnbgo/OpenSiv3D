# serial library

# Platform-specific sources
if(WIN32)
  set(SERIAL_PLATFORM_SOURCES
    impl/win.cc
    impl/list_ports/list_ports_win.cc
  )
elseif(IOS)
  # iOS: IOKit が無いのでポートの列挙は Linux 版 (/sys を探す。iOS では常に空になる) を使う
  set(SERIAL_PLATFORM_SOURCES
    impl/unix.cc
    impl/list_ports/list_ports_linux.cc
  )
elseif(APPLE)
  set(SERIAL_PLATFORM_SOURCES
    impl/unix.cc
    impl/list_ports/list_ports_osx.cc
  )
else()
  set(SERIAL_PLATFORM_SOURCES
    impl/unix.cc
    impl/list_ports/list_ports_linux.cc
  )
endif()

add_library(Siv3D_ThirdParty_serial OBJECT
  serial.cc
  ${SERIAL_PLATFORM_SOURCES}
)
target_include_directories(Siv3D_ThirdParty_serial PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(Siv3D_ThirdParty_serial PRIVATE cxx_std_20)
set_target_properties(Siv3D_ThirdParty_serial PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
