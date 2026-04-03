# RecastDetour library
add_library(Siv3D_ThirdParty_RecastDetour OBJECT
  DetourAlloc.cpp
  DetourAssert.cpp
  DetourCommon.cpp
  DetourNavMesh.cpp
  DetourNavMeshBuilder.cpp
  DetourNavMeshQuery.cpp
  DetourNode.cpp
  Recast.cpp
  RecastAlloc.cpp
  RecastArea.cpp
  RecastAssert.cpp
  RecastContour.cpp
  RecastFilter.cpp
  RecastLayers.cpp
  RecastMesh.cpp
  RecastMeshDetail.cpp
  RecastRasterization.cpp
  RecastRegion.cpp
)
target_include_directories(Siv3D_ThirdParty_RecastDetour PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(Siv3D_ThirdParty_RecastDetour PRIVATE cxx_std_20)
set_target_properties(Siv3D_ThirdParty_RecastDetour PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
