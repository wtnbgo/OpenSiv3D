# skia library

# Platform-specific sources
if(WIN32)
  set(SKIA_PLATFORM_SOURCES
    src/ports/SkDebug_win.cpp
  )
elseif(ANDROID)
  # Android は SkDebug_stdio.cpp が対象外 (SK_BUILD_FOR_ANDROID) なので logcat 版
  set(SKIA_PLATFORM_SOURCES
    src/ports/SkDebug_android.cpp
  )
else()
  set(SKIA_PLATFORM_SOURCES
    src/ports/SkDebug_stdio.cpp
  )
endif()

add_library(Siv3D_ThirdParty_skia OBJECT
  src/base/SkArenaAlloc.cpp
  src/base/SkBezierCurves.cpp
  src/base/SkBlockAllocator.cpp
  src/base/SkBuffer.cpp
  src/base/SkContainers.cpp
  src/base/SkCubics.cpp
  src/base/SkDeque.cpp
  src/base/SkFloatingPoint.cpp
  src/base/SkHalf.cpp
  src/base/SkMalloc.cpp
  src/base/SkMathPriv.cpp
  src/base/SkQuads.cpp
  src/base/SkSafeMath.cpp
  src/base/SkSemaphore.cpp
  src/base/SkSharedMutex.cpp
  src/base/SkSpinlock.cpp
  src/base/SkTDArray.cpp
  src/base/SkThreadID.cpp
  src/base/SkTSearch.cpp
  src/base/SkUTF.cpp
  src/base/SkUtils.cpp
  src/core/SkCubicClipper.cpp
  src/core/SkEdgeClipper.cpp
  src/core/SkGeometry.cpp
  src/core/SkIDChangeListener.cpp
  src/core/SkLineClipper.cpp
  src/core/SkM44.cpp
  src/core/SkMatrix.cpp
  src/core/SkMatrixInvert.cpp
  src/core/SkPath.cpp
  src/core/SkPathBuilder.cpp
  src/core/SkPathRef.cpp
  src/core/SkPoint.cpp
  src/core/SkRect.cpp
  src/core/SkRRect.cpp
  src/core/SkString.cpp
  src/core/SkStringUtils.cpp
  src/pathops/SkAddIntersections.cpp
  src/pathops/SkDConicLineIntersection.cpp
  src/pathops/SkDCubicLineIntersection.cpp
  src/pathops/SkDCubicToQuads.cpp
  src/pathops/SkDLineIntersection.cpp
  src/pathops/SkDQuadLineIntersection.cpp
  src/pathops/SkIntersections.cpp
  src/pathops/SkOpAngle.cpp
  src/pathops/SkOpBuilder.cpp
  src/pathops/SkOpCoincidence.cpp
  src/pathops/SkOpContour.cpp
  src/pathops/SkOpCubicHull.cpp
  src/pathops/SkOpEdgeBuilder.cpp
  src/pathops/SkOpSegment.cpp
  src/pathops/SkOpSpan.cpp
  src/pathops/SkPathOpsAsWinding.cpp
  src/pathops/SkPathOpsCommon.cpp
  src/pathops/SkPathOpsConic.cpp
  src/pathops/SkPathOpsCubic.cpp
  src/pathops/SkPathOpsCurve.cpp
  src/pathops/SkPathOpsDebug.cpp
  src/pathops/SkPathOpsLine.cpp
  src/pathops/SkPathOpsOp.cpp
  src/pathops/SkPathOpsQuad.cpp
  src/pathops/SkPathOpsRect.cpp
  src/pathops/SkPathOpsSimplify.cpp
  src/pathops/SkPathOpsTightBounds.cpp
  src/pathops/SkPathOpsTSect.cpp
  src/pathops/SkPathOpsTypes.cpp
  src/pathops/SkPathOpsWinding.cpp
  src/pathops/SkPathWriter.cpp
  src/pathops/SkReduceOrder.cpp
  src/ports/SkMemory_malloc.cpp
  ${SKIA_PLATFORM_SOURCES}
)
target_include_directories(Siv3D_ThirdParty_skia 
  PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}
)
target_compile_features(Siv3D_ThirdParty_skia PRIVATE cxx_std_20)
set_target_properties(Siv3D_ThirdParty_skia PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
