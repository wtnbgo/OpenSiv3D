// cpuinfo_x86.c は x86 以外では #error になるため、x86 のときだけ取り込む
// (CMake の変数ではなくコンパイラの判定で切り替えるので、ユニバーサルバイナリでも正しく動く)
#if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#include "cpuinfo_x86.c"
#endif
