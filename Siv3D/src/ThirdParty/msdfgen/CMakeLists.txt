# msdfgen library
add_library(Siv3D_ThirdParty_msdfgen OBJECT
  core/contour-combiners.cpp
  core/Contour.cpp
  core/edge-coloring.cpp
  core/edge-segments.cpp
  core/edge-selectors.cpp
  core/EdgeHolder.cpp
  core/equation-solver.cpp
  core/msdf-error-correction.cpp
  core/MSDFErrorCorrection.cpp
  core/msdfgen.cpp
  core/Projection.cpp
  core/rasterization.cpp
  core/render-sdf.cpp
  core/save-bmp.cpp
  core/save-tiff.cpp
  core/Scanline.cpp
  core/sdf-error-estimation.cpp
  core/shape-description.cpp
  core/Shape.cpp
  core/SignedDistance.cpp
  core/Vector2.cpp
  ext/resolve-shape-geometry.cpp
)
target_include_directories(Siv3D_ThirdParty_msdfgen PUBLIC 
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../skia
)
target_compile_features(Siv3D_ThirdParty_msdfgen PRIVATE cxx_std_20)
set_target_properties(Siv3D_ThirdParty_msdfgen PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
