# muparser library
add_library(Siv3D_ThirdParty_muparser OBJECT
  muParser.cpp
  muParserBase.cpp
  muParserBytecode.cpp
  muParserCallback.cpp
  muParserError.cpp
  muParserInt.cpp
  muParserTokenReader.cpp
)
target_include_directories(Siv3D_ThirdParty_muparser PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(Siv3D_ThirdParty_muparser PRIVATE cxx_std_20)
target_compile_definitions(Siv3D_ThirdParty_muparser PRIVATE MUPARSER_STATIC)
set_target_properties(Siv3D_ThirdParty_muparser PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
