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

# include <Siv3D/Common.hpp>
# include <Siv3D/Time.hpp>

# if SIV3D_PLATFORM(WINDOWS)
#	include <Siv3D/Windows/Windows.hpp>
# else
#	include <time.h>
#	include <sys/time.h>
# endif

namespace s3d
{
	namespace detail
	{
	# if SIV3D_PLATFORM(WINDOWS)

		static uint64 GetQPCFrequency() noexcept
		{
			LARGE_INTEGER frequency;
			::QueryPerformanceFrequency(&frequency);
			return frequency.QuadPart;
		}

		static uint64 GetQPCCount() noexcept
		{
			LARGE_INTEGER counter;
			::QueryPerformanceCounter(&counter);
			return counter.QuadPart;
		}

		const static uint64 g_QPCFrequency = GetQPCFrequency();
		const static uint64 g_BaseCount = GetQPCCount();

		inline uint64 GetApplicationTimeNS() noexcept
		{
			const uint64 delta = GetQPCCount() - g_BaseCount;
			return (delta * 1'000'000'000ULL / g_QPCFrequency);
		}

	# else

		inline uint64 GetTimeNS() noexcept
		{
			timespec ts;
			clock_gettime(CLOCK_MONOTONIC, &ts);
			return ((ts.tv_sec * 1'000'000'000ULL) + ts.tv_nsec);
		}

		const static uint64 g_BaseTimeNS = GetTimeNS();

		inline uint64 GetApplicationTimeNS() noexcept
		{
			return (GetTimeNS() - g_BaseTimeNS);
		}

	# endif
	}

	namespace Time
	{
		uint64 GetSec() noexcept
		{
			return (detail::GetApplicationTimeNS() / 1'000'000'000);
		}

		uint64 GetMillisec() noexcept
		{
			return (detail::GetApplicationTimeNS() / 1'000'000);
		}

		uint64 GetMicrosec() noexcept
		{
			return (detail::GetApplicationTimeNS() / 1'000);
		}

		uint64 GetNanosec() noexcept
		{
			return detail::GetApplicationTimeNS();
		}

		uint64 GetSecSinceEpoch()
		{
			return (GetMicrosecSinceEpoch() / 1'000'000);
		}

		uint64 GetMillisecSinceEpoch()
		{
			return (GetMicrosecSinceEpoch() / 1'000);
		}

		uint64 GetMicrosecSinceEpoch()
		{
		# if SIV3D_PLATFORM(WINDOWS)
			FILETIME ft;
			::GetSystemTimeAsFileTime(&ft);
			uint64 t = (static_cast<uint64>(ft.dwHighDateTime) << 32) | ft.dwLowDateTime;
			// FILETIME epoch is 1601-01-01, Unix epoch is 1970-01-01
			// Difference: 11644473600 seconds = 116444736000000000 * 100ns
			t -= 116444736000000000ULL;
			return (t / 10); // 100ns -> us
		# else
			::timeval tv;
			::gettimeofday(&tv, nullptr);
			return (tv.tv_sec * 1'000'000ULL + tv.tv_usec);
		# endif
		}

		int32 UTCOffsetMinutes()
		{
		# if SIV3D_PLATFORM(WINDOWS)
			TIME_ZONE_INFORMATION tzi;
			if (::GetTimeZoneInformation(&tzi) == TIME_ZONE_ID_INVALID)
			{
				return 0;
			}
			return -tzi.Bias;
		# else
			struct timeval tv;
			struct timezone tz;
			::gettimeofday(&tv, &tz);
			return -tz.tz_minuteswest;
		# endif
		}
	}
}
