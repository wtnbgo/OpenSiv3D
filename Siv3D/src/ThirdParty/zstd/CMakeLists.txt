# zstd library
add_library(Siv3D_ThirdParty_zstd OBJECT
  common/debug.c
  common/entropy_common.c
  common/error_private.c
  common/fse_decompress.c
  common/pool.c
  common/threading.c
  common/xxhash.c
  common/zstd_common.c
  compress/fse_compress.c
  compress/hist.c
  compress/huf_compress.c
  compress/zstdmt_compress.c
  compress/zstd_compress.c
  compress/zstd_compress_literals.c
  compress/zstd_compress_sequences.c
  compress/zstd_compress_superblock.c
  compress/zstd_double_fast.c
  compress/zstd_fast.c
  compress/zstd_lazy.c
  compress/zstd_ldm.c
  compress/zstd_opt.c
  decompress/huf_decompress.c
  # decompress/huf_decompress_amd64.S  # Assembly file - handle separately if needed
  decompress/zstd_ddict.c
  decompress/zstd_decompress.c
  decompress/zstd_decompress_block.c
  dictBuilder/cover.c
  dictBuilder/divsufsort.c
  dictBuilder/fastcover.c
  dictBuilder/zdict.c
)
target_include_directories(Siv3D_ThirdParty_zstd PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/..)
target_compile_features(Siv3D_ThirdParty_zstd PRIVATE c_std_11)
# huf_decompress_amd64.S を組み込まないので、gcc / clang (x86_64) でも C 実装を使わせる
# (MSVC は元々 asm 経路を使わない)
target_compile_definitions(Siv3D_ThirdParty_zstd PRIVATE ZSTD_DISABLE_ASM)
set_target_properties(Siv3D_ThirdParty_zstd PROPERTIES
  C_EXTENSIONS OFF
  DEBUG_POSTFIX "d"
)
