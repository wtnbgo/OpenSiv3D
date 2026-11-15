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

# include <iostream>
# include <filesystem>
# include <Siv3D/Common/Siv3DEngine.hpp>
# include <Siv3D/System/ISystem.hpp>
# include <Siv3D/Error.hpp>
# include <Siv3D/EngineLog.hpp>
# include <Siv3D/Unicode.hpp>
# include <Siv3D/FileSystem.hpp>

# if defined(__ANDROID__)
	// Android: SDL の Java 側 (SDLActivity) が libmain.so の SDL_main を呼ぶので、main を SDL_main にする
	// (アプリの共有ライブラリは -Wl,--undefined=SDL_main でこの関数を取り込む。siv3d_add_app を参照)
#	include <SDL3/SDL_main.h>
# endif

void Main();

namespace s3d::detail::init
{
	void InitCommandLines(int argc, char** argv);
	void InitModulePath(const char* arg);
	void SetCustomResourceDirectory(const FilePath& path);
}

int main(int argc, char* argv[])
{
	using namespace s3d;
	std::clog << "OpenSiv3D for " << Unicode::Narrow(SIV3D_PLATFORM_NAME) << " (SDL3)\n";

	detail::init::InitCommandLines(argc, argv);
	detail::init::InitModulePath(argv[0]);

	// 最初のコマンドライン引数をアプリケーションディレクトリとして設定
	if (argc >= 2)
	{
		const std::filesystem::path appDirectory{ argv[1] };

		// カレントディレクトリを指定パスに変更
		std::error_code ec;
		std::filesystem::current_path(appDirectory, ec);

		// 指定パス内の resources/ をリソースディレクトリとして設定
		detail::init::SetCustomResourceDirectory(Unicode::Widen((appDirectory / "resources").string()));
	}
# if SIV3D_PLATFORM(MACOS)
	else
	{
		// .app バンドルとして起動された場合 (Finder / open) カレントディレクトリは "/" になるため、
		// 実行ファイルのディレクトリ (Contents/MacOS/) をカレントディレクトリにする
		const std::filesystem::path modulePath{ FileSystem::ModulePath().toUTF8() };

		if (modulePath.parent_path().string().ends_with(".app/Contents/MacOS"))
		{
			std::error_code ec;
			std::filesystem::current_path(modulePath.parent_path(), ec);
		}
	}
# endif

	Siv3DEngine engine;
	
	try
	{
		SIV3D_ENGINE(System)->init();
	}
	catch (const Error& error)
	{
		std::cerr << error << '\n';
		return -1;
	}

	LOG_TRACE(U"Main() ---");

	Main();

	LOG_TRACE(U"--- Main()");

	return 0;
}
