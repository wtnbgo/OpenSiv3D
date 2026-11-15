//-----------------------------------------------
//
//	This file is part of the Siv3D Engine.
//
//	Copyright (c) 2008-2025 Ryo Suzuki
//	Copyright (c) 2016-2025 OpenSiv3D Project
//
//	Licensed under the MIT License.
//
//-----------------------------------------------

# pragma once
# include "Platform.hpp"

# define SIMDE_ENABLE_NATIVE_ALIASES

# if !SIV3D_INTRINSIC(SSE)

# include <ThirdParty/simde/x86/sse.h>
# include <ThirdParty/simde/x86/sse2.h>
# include <ThirdParty/simde/x86/sse3.h>
# include <ThirdParty/simde/x86/ssse3.h>
# include <ThirdParty/simde/x86/sse4.1.h>
# include <ThirdParty/simde/x86/sse4.2.h>

# if defined(__APPLE__)

// macOS / iOS には <malloc.h> と memalign が無いので posix_memalign を使う
# include <cstdlib>

inline void* Siv3D_SIMD_AlignedMalloc(const size_t size, const size_t alignment) noexcept
{
	void* p = nullptr;
	return (::posix_memalign(&p, ((alignment < sizeof(void*)) ? sizeof(void*) : alignment), size) == 0) ? p : nullptr;
}

# define _mm_malloc(__size, __align) Siv3D_SIMD_AlignedMalloc((__size), (__align))
# define _mm_free free

# else

# include <malloc.h>    // For memalign

# define _mm_malloc(__size, __align) memalign((__align), (__size))
# define _mm_free free

# endif

# else

# if __has_include(<xmmintrin.h>)
#	include <xmmintrin.h>
# else
#	include <ThirdParty/simde/x86/sse.h>
# endif

# if __has_include(<emmintrin.h>)
#	include <emmintrin.h>
# else
#	include <ThirdParty/simde/x86/sse2.h>
# endif

# if __has_include(<pmmintrin.h>)
#	include <pmmintrin.h>
# else
#	include <ThirdParty/simde/x86/sse3.h>
# endif

# if __has_include(<tmmintrin.h>)
#	include <tmmintrin.h>
# else
#	include <ThirdParty/simde/x86/ssse3.h>
# endif

# if __has_include(<smmintrin.h>)
#	include <smmintrin.h>
# else
#	include <ThirdParty/simde/x86/sse4.1.h>
# endif

# if __has_include(<nmmintrin.h>)
#	include <nmmintrin.h>
# else
#	include <ThirdParty/simde/x86/sse4.2.h>
# endif

# endif
