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
# include <Siv3D/System.hpp>

# if SIV3D_PLATFORM(WINDOWS)
#	include <Siv3D/Windows/Windows.hpp>
# else
#	include <unistd.h>
# endif

namespace s3d
{
	namespace System
	{
		void Sleep(const int32 milliseconds)
		{
			if (milliseconds < 0)
			{
				return;
			}

		# if SIV3D_PLATFORM(WINDOWS)
			::Sleep(static_cast<DWORD>(milliseconds));
		# else
			::usleep(static_cast<uint32>(milliseconds) * 1000);
		# endif
		}
	}
}
