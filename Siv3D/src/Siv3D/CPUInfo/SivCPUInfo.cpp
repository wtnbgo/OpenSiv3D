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

# include <cstring>
# include <Siv3D/CPUInfo.hpp>
# include <Siv3D/Unicode.hpp>

# if defined(__x86_64__) || defined(_M_X64) || defined(__i386__) || defined(_M_IX86)
#	define SIV3D_CPU_X86 1
#	include <ThirdParty/cpu_features/cpuinfo_x86.h>
# else
#	define SIV3D_CPU_X86 0
#	if defined(__APPLE__)
#		include <sys/sysctl.h>
#	endif
# endif

namespace s3d
{
	namespace detail
	{
	# if SIV3D_CPU_X86

		[[nodiscard]]
		static CPUInfo InitCPUInfo() noexcept
		{
			static_assert(sizeof(s3d::X86Features) == sizeof(cpu_features::X86Features));

			const cpu_features::X86Info x86Info = cpu_features::GetX86Info();
			
			char brand[49] = {};
			cpu_features::FillX86BrandString(brand);

			CPUInfo result;
			std::memcpy(&result.features, &x86Info.features, sizeof(s3d::X86Features));
			result.family	= x86Info.family;
			result.model	= x86Info.model;
			result.stepping	= x86Info.stepping;
			result.vendor	= Unicode::Widen(x86Info.vendor);
			result.brand	= Unicode::Widen(brand);

			return result;
		}

	# else

		// x86 以外 (arm64 など): X86Features はすべて 0。vendor / brand は取れる範囲で設定する
		[[nodiscard]]
		static CPUInfo InitCPUInfo() noexcept
		{
			CPUInfo result;
			std::memset(&result.features, 0, sizeof(result.features));
			result.family	= 0;
			result.model	= 0;
			result.stepping	= 0;

		# if defined(__APPLE__)
			{
				char brand[256] = {};
				size_t size = sizeof(brand);

				if (::sysctlbyname("machdep.cpu.brand_string", brand, &size, nullptr, 0) == 0)
				{
					result.brand = Unicode::Widen(brand);
				}
			}
			result.vendor = U"Apple";
		# elif defined(__aarch64__) || defined(__arm__)
			result.vendor = U"ARM";
		# endif

			return result;
		}

	# endif
	}

	const CPUInfo g_CPUInfo = detail::InitCPUInfo();

	const CPUInfo& GetCPUInfo() noexcept
	{
		return g_CPUInfo;
	}
}
