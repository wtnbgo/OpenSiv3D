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
	std::clog << "OpenSiv3D for Linux\n";

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
