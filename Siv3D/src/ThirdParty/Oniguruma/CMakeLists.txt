# Oniguruma library
add_library(Siv3D_ThirdParty_Oniguruma OBJECT
  ascii.c
  big5.c
  cp1251.c
  euc_jp.c
  euc_jp_prop.c
  euc_kr.c
  euc_tw.c
  gb18030.c
  iso8859_1.c
  iso8859_10.c
  iso8859_11.c
  iso8859_13.c
  iso8859_14.c
  iso8859_15.c
  iso8859_16.c
  iso8859_2.c
  iso8859_3.c
  iso8859_4.c
  iso8859_5.c
  iso8859_6.c
  iso8859_7.c
  iso8859_8.c
  iso8859_9.c
  koi8.c
  koi8_r.c
  onig_init.c
  regcomp.c
  regenc.c
  regerror.c
  regexec.c
  reggnu.c
  regparse.c
  regsyntax.c
  regtrav.c
  regversion.c
  sjis.c
  sjis_prop.c
  st.c
  unicode.c
  unicode_fold1_key.c
  unicode_fold2_key.c
  unicode_fold3_key.c
  unicode_unfold_key.c
  utf16_be.c
  utf16_le.c
  utf32_be.c
  utf32_le.c
  utf8.c
)
target_include_directories(Siv3D_ThirdParty_Oniguruma PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_definitions(Siv3D_ThirdParty_Oniguruma PUBLIC ONIG_STATIC)
target_compile_features(Siv3D_ThirdParty_Oniguruma PRIVATE c_std_11)
set_target_properties(Siv3D_ThirdParty_Oniguruma PROPERTIES
  C_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
