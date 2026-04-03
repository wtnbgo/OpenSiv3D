# lunasvg library
add_library(Siv3D_ThirdParty_lunasvg OBJECT
  canvas.cpp
  clippathelement.cpp
  defselement.cpp
  element.cpp
  gelement.cpp
  geometryelement.cpp
  graphicselement.cpp
  layoutcontext.cpp
  lunasvg.cpp
  markerelement.cpp
  maskelement.cpp
  paintelement.cpp
  parser.cpp
  property.cpp
  stopelement.cpp
  styledelement.cpp
  styleelement.cpp
  svgelement.cpp
  symbolelement.cpp
  useelement.cpp
)
target_include_directories(Siv3D_ThirdParty_lunasvg PUBLIC 
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include
)
target_compile_features(Siv3D_ThirdParty_lunasvg PRIVATE cxx_std_20)
set_target_properties(Siv3D_ThirdParty_lunasvg PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
