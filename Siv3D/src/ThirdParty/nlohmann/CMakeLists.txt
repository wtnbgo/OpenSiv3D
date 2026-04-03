# nlohmann library (json-schema-validator)
add_library(Siv3D_ThirdParty_nlohmann OBJECT
  json-patch.cpp
  json-schema-draft7.json.cpp
  json-uri.cpp
  json-validator.cpp
  smtp-address-validator.cpp
  string-format-check.cpp
)
target_include_directories(Siv3D_ThirdParty_nlohmann PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(Siv3D_ThirdParty_nlohmann PRIVATE cxx_std_20)
set_target_properties(Siv3D_ThirdParty_nlohmann PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
