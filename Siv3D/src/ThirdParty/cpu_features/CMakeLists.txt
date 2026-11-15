# cpu_features library
add_library(Siv3D_ThirdParty_cpu_features OBJECT
  cpuinfo_x86_if_x86.c
  filesystem.c
  stack_line_reader.c
  string_view.c
)
target_include_directories(Siv3D_ThirdParty_cpu_features PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(Siv3D_ThirdParty_cpu_features PRIVATE c_std_11)
set_target_properties(Siv3D_ThirdParty_cpu_features PROPERTIES
  C_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
