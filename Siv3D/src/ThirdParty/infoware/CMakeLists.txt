# infoware library

# Common sources (all platforms)
set(INFOWARE_COMMON_SOURCES
  src/cpu/endianness/all.cpp
  src/cpu/instruction_set/instruction_set.cpp
  src/cpu/vendor_model_name/vendor_id.cpp
  src/detail/scope.cpp
  src/system/displays/displays_default_blank.cpp
)

# Platform-specific sources
if(WIN32)
  set(INFOWARE_PLATFORM_SOURCES
    src/cpu/architecture/architecture_windows.cpp
    src/cpu/frequency/frequency_windows.cpp
    src/cpu/instruction_set/instruction_set_windows.cpp
    src/cpu/quantities_cache/quantities_cache_windows.cpp
    src/cpu/vendor_model_name/vendor_model_name_windows.cpp
    src/detail/cpuid_windows.cpp
    src/detail/sysctl_non_darwin.cpp
    src/detail/winstring_windows.cpp
    src/system/amounts/amounts_windows.cpp
    src/system/displays/displays_windows.cpp
    src/system/kernel_info/kernel_info_windows.cpp
    src/system/memory/memory_windows.cpp
    src/system/OS_info/os_info_windows.cpp
  )
elseif(APPLE)
  set(INFOWARE_PLATFORM_SOURCES
    src/cpu/architecture/architecture_non_windows.cpp
    src/cpu/frequency/frequency_darwin.cpp
    src/cpu/instruction_set/instruction_set_darwin.cpp
    src/cpu/quantities_cache/quantities_cache_darwin.cpp
    src/cpu/vendor_model_name/vendor_model_name_darwin.cpp
    src/detail/cpuid_non_windows.cpp
    src/detail/sysctl_darwin.cpp
    src/detail/winstring_non_windows.cpp
    src/system/amounts/amounts_non_windows.cpp
    src/system/displays/displays_darwin.cpp
    src/system/kernel_info/kernel_info_non_windows.cpp
    src/system/memory/memory_darwin.cpp
    src/system/OS_info/os_info_darwin.cpp
  )
else()
  # Linux and other Unix-like systems
  set(INFOWARE_PLATFORM_SOURCES
    src/cpu/architecture/architecture_non_windows.cpp
    src/cpu/frequency/frequency_non_windows_non_darwin.cpp
    src/cpu/instruction_set/instruction_set_non_windows_non_darwin.cpp
    src/cpu/quantities_cache/quantities_cache_non_windows_non_darwin.cpp
    src/cpu/vendor_model_name/vendor_model_name_non_windows_non_darwin.cpp
    src/detail/cpuid_non_windows.cpp
    src/detail/sysctl_non_darwin.cpp
    src/detail/winstring_non_windows.cpp
    src/system/amounts/amounts_non_windows.cpp
    src/system/displays/displays_x11.cpp
    src/system/kernel_info/kernel_info_non_windows.cpp
    src/system/memory/memory_non_windows_non_darwin.cpp
    src/system/OS_info/os_info_non_windows_non_darwin.cpp
  )
endif()

add_library(Siv3D_ThirdParty_infoware OBJECT
  ${INFOWARE_COMMON_SOURCES}
  ${INFOWARE_PLATFORM_SOURCES}
)
target_include_directories(Siv3D_ThirdParty_infoware PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(Siv3D_ThirdParty_infoware PRIVATE cxx_std_20)
set_target_properties(Siv3D_ThirdParty_infoware PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
