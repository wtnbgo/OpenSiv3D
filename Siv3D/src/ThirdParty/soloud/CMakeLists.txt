# soloud library
add_library(Siv3D_ThirdParty_soloud OBJECT
  src/audiosource/monotone/soloud_monotone.cpp
  src/audiosource/noise/soloud_noise.cpp
  src/audiosource/sfxr/soloud_sfxr.cpp
  src/audiosource/speech/darray.cpp
  src/audiosource/speech/klatt.cpp
  src/audiosource/speech/resonator.cpp
  src/audiosource/speech/soloud_speech.cpp
  src/audiosource/speech/tts.cpp
  src/audiosource/tedsid/sid.cpp
  src/audiosource/tedsid/soloud_tedsid.cpp
  src/audiosource/tedsid/ted.cpp
  src/audiosource/vic/soloud_vic.cpp
  src/audiosource/vizsn/soloud_vizsn.cpp
  src/audiosource/wav/dr_impl.cpp
  src/audiosource/wav/soloud_wav.cpp
  src/audiosource/wav/soloud_wavstream.cpp
  src/audiosource/wav/stb_vorbis.c
  src/backend/miniaudio/soloud_miniaudio.cpp
  src/backend/nosound/soloud_nosound.cpp
  src/core/soloud.cpp
  src/core/soloud_audiosource.cpp
  src/core/soloud_bus.cpp
  src/core/soloud_core_3d.cpp
  src/core/soloud_core_basicops.cpp
  src/core/soloud_core_faderops.cpp
  src/core/soloud_core_filterops.cpp
  src/core/soloud_core_getters.cpp
  src/core/soloud_core_setters.cpp
  src/core/soloud_core_voicegroup.cpp
  src/core/soloud_core_voiceops.cpp
  src/core/soloud_fader.cpp
  src/core/soloud_fft.cpp
  src/core/soloud_fft_lut.cpp
  src/core/soloud_file.cpp
  src/core/soloud_filter.cpp
  src/core/soloud_misc.cpp
  src/core/soloud_queue.cpp
  src/core/soloud_thread.cpp
  src/filter/soloud_biquadresonantfilter.cpp
  src/filter/soloud_dcremovalfilter.cpp
  src/filter/soloud_echofilter.cpp
  src/filter/soloud_eqfilter.cpp
  src/filter/soloud_fftfilter.cpp
  src/filter/soloud_flangerfilter.cpp
  src/filter/soloud_freeverbfilter.cpp
  src/filter/soloud_lofifilter.cpp
)
target_include_directories(Siv3D_ThirdParty_soloud 
  PUBLIC 
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_compile_features(Siv3D_ThirdParty_soloud PRIVATE cxx_std_20 c_std_11)
target_compile_definitions(Siv3D_ThirdParty_soloud PRIVATE
  WITH_MINIAUDIO
  WITH_NOSOUND
)
if(APPLE)
  # miniaudio 0.10 は CoreAudio 系フレームワークを相対パス ("CoreFoundation.framework/CoreFoundation" など) で
  # dlopen するが、現行の macOS では解決できず "No backend" になり NoSound にフォールバックする。
  # 実行時リンクをやめてフレームワークを直接リンクする (リンクはトップの CMakeLists.txt で Siv3D に追加)
  target_compile_definitions(Siv3D_ThirdParty_soloud PRIVATE MA_NO_RUNTIME_LINKING)
endif()
if(IOS)
  # iOS の miniaudio は AVAudioSession (AVFoundation、Objective-C) を使うので Objective-C++ としてコンパイルする
  set_source_files_properties(src/backend/miniaudio/soloud_miniaudio.cpp PROPERTIES LANGUAGE OBJCXX)
endif()
set_target_properties(Siv3D_ThirdParty_soloud PROPERTIES
  CXX_EXTENSIONS OFF
  C_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
