# plutovg library
add_library(Siv3D_ThirdParty_plutovg OBJECT
  plutovg-blend.c
  plutovg-dash.c
  plutovg-geometry.c
  plutovg-paint.c
  plutovg-rle.c
  plutovg.c
  plutovg-ft-math.c
  plutovg-ft-raster.c
  plutovg-ft-stroker.c
)
target_include_directories(Siv3D_ThirdParty_plutovg PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(Siv3D_ThirdParty_plutovg PRIVATE c_std_11)
set_target_properties(Siv3D_ThirdParty_plutovg PROPERTIES
  C_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
