# angelscript library
if(MSVC)
  enable_language(ASM_MASM)
endif()
add_library(Siv3D_ThirdParty_angelscript OBJECT
  as_atomic.cpp
  as_builder.cpp
  as_bytecode.cpp
  as_callfunc.cpp
  as_callfunc_arm.cpp
  as_callfunc_arm64.cpp
  as_callfunc_mips.cpp
  as_callfunc_ppc.cpp
  as_callfunc_ppc_64.cpp
  as_callfunc_sh4.cpp
  as_callfunc_x64_gcc.cpp
  as_callfunc_x64_mingw.cpp
  as_callfunc_x64_msvc.cpp
  as_callfunc_x86.cpp
  as_callfunc_xenon.cpp
  as_compiler.cpp
  as_configgroup.cpp
  as_context.cpp
  as_datatype.cpp
  as_gc.cpp
  as_generic.cpp
  as_globalproperty.cpp
  as_memory.cpp
  as_module.cpp
  as_objecttype.cpp
  as_outputbuffer.cpp
  as_parser.cpp
  as_restore.cpp
  as_scriptcode.cpp
  as_scriptengine.cpp
  as_scriptfunction.cpp
  as_scriptnode.cpp
  as_scriptobject.cpp
  as_string.cpp
  as_string_util.cpp
  as_thread.cpp
  as_tokenizer.cpp
  as_typeinfo.cpp
  as_variablescope.cpp
)
if(MSVC)
  target_sources(Siv3D_ThirdParty_angelscript PRIVATE as_callfunc_x64_msvc_asm.asm)
endif()
target_include_directories(Siv3D_ThirdParty_angelscript PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/..
  ${CMAKE_CURRENT_SOURCE_DIR}/../../../include/ThirdParty
)
target_compile_features(Siv3D_ThirdParty_angelscript PRIVATE cxx_std_20)
target_compile_definitions(Siv3D_ThirdParty_angelscript PRIVATE AS_USE_NAMESPACE)
set_target_properties(Siv3D_ThirdParty_angelscript PROPERTIES
  CXX_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
