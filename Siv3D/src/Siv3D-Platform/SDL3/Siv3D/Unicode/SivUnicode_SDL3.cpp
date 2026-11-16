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

# include <Siv3D/Unicode.hpp>
# include <Siv3D/String.hpp>

namespace s3d
{
	namespace Unicode
	{
		String Widen(const std::string_view s)
		{
			return FromUTF8(s);
		}
	
		String FromWstring(const std::wstring_view s)
		{
		# if SIV3D_PLATFORM(WINDOWS)
			// Windows の wchar_t は UTF-16 (サロゲートペアを含む) なので、1 文字ずつのコピーではなく UTF-16 として変換する
			return FromUTF16(std::u16string_view{ reinterpret_cast<const char16_t*>(s.data()), s.size() });
		# else
			return String(s.begin(), s.end());
		# endif
		}
	
		std::string Narrow(const StringView s)
		{
			return ToUTF8(s);
		}

		std::wstring ToWstring(const StringView s)
		{
		# if SIV3D_PLATFORM(WINDOWS)
			const std::u16string utf16 = ToUTF16(s);
			return std::wstring(utf16.begin(), utf16.end());
		# else
			return std::wstring(s.begin(), s.end());
		# endif
		}
	}
}
