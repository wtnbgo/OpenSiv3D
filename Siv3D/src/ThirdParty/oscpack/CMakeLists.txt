# oscpack library

# Platform-specific sources
if(WIN32)
  set(OSCPACK_PLATFORM_SOURCES
    ip/win32/NetworkingUtils.cpp
    ip/win32/UdpSocket.cpp
  )
else()
  set(OSCPACK_PLATFORM_SOURCES
    ip/posix/NetworkingUtils.cpp
    ip/posix/UdpSocket.cpp
  )
endif()

add_library(Siv3D_ThirdParty_oscpack OBJECT
  ip/IpEndpointName.cpp
  osc/OscOutboundPacketStream.cpp
  osc/OscPrintReceivedElements.cpp
  osc/OscReceivedElements.cpp
  osc/OscTypes.cpp
  ${OSCPACK_PLATFORM_SOURCES}
)
target_include_directories(Siv3D_ThirdParty_oscpack PUBLIC 
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/ThirdParty
)
target_compile_features(Siv3D_ThirdParty_oscpack PRIVATE cxx_std_20)
set_target_properties(Siv3D_ThirdParty_oscpack PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
