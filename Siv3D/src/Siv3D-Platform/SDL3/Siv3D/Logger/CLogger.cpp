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

# include <array>
# include <cstdlib>
# include <filesystem>
# include <iostream>
# include <Siv3D/String.hpp>
# include <Siv3D/FormatLiteral.hpp>
# include <Siv3D/LogType.hpp>
# include <Siv3D/Time.hpp>
# include "CLogger.hpp"
# if defined(__ANDROID__)
#	include <android/log.h>
# endif
# if SIV3D_PLATFORM(WINDOWS)
#	include <Siv3D/Windows/Windows.hpp>
# endif

namespace s3d
{
	namespace detail
	{
		constexpr std::array<StringView, 7> LogTypeNames =
		{
			U"[error]   "_sv,
			U"[fail]    "_sv,
			U"[warning] "_sv,
			U""_sv,
			U"[info]    "_sv,
			U"[trace]   "_sv,
			U"[verbose] "_sv,
		};
	}

	CLogger::CLogger()
	{
		// SIV3D_LOG_FILE=<パス> でログをファイルにも書く (コンソールの無い Windows の GUI アプリや、自動テストで使う)。
		// エンジンの初期化より前に作られるので、FileSystem などは使わずに環境変数を直接読む
	# if SIV3D_PLATFORM(WINDOWS)
		if (const wchar_t* path = ::_wgetenv(L"SIV3D_LOG_FILE"); path && *path)
		{
			m_file.open(std::filesystem::path{ path }, std::ios::out | std::ios::trunc | std::ios::binary);
		}
	# else
		if (const char* path = std::getenv("SIV3D_LOG_FILE"); path && *path)
		{
			m_file.open(std::filesystem::path{ path }, std::ios::out | std::ios::trunc | std::ios::binary);
		}
	# endif
	}

	CLogger::~CLogger()
	{
	
	}

	void CLogger::write(const LogType type, const StringView s)
	{
		if (not m_enabled)
		{
			return;
		}

		const int64 timeStamp = Time::GetMillisec();
		const StringView logTypeName = detail::LogTypeNames[FromEnum(type)];
		const String text = U"{}: {}"_fmt(timeStamp, logTypeName) + s;
		const std::string output = text.narrow();

		std::lock_guard lock{ m_mutex };
		{
			if (m_file.is_open())
			{
				// UTF-8 で 1 行ずつ書き、すぐに flush する (異常終了しても直前までのログが残るように)
				m_file << text.toUTF8() << '\n';
				m_file.flush();
			}

		# if SIV3D_PLATFORM(WEB)
			std::cout << output << std::endl;
		# elif defined(__ANDROID__)
			// Android は標準出力が捨てられるので logcat に出す (adb logcat -s Siv3D)
			__android_log_write(((type == LogType::Error) ? ANDROID_LOG_ERROR : ANDROID_LOG_INFO), "Siv3D", output.c_str());
		# else
			std::clog << output << std::endl;
		# endif
		}
	}

	void CLogger::setEnabled(const bool enabled)
	{
		m_enabled = enabled;
	}	
}
