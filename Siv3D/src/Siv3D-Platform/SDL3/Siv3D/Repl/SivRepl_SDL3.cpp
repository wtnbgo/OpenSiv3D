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

//
// 簡易 REPL (開発・自動テスト用): 環境変数 SIV3D_REPL_FILE=<パス> を指定すると、毎フレームそのファイルを見て、
// 書かれた命令を 1 行ずつ実行する。実行し終えたらファイルを消すので、消えたことで受け付けたと分かる。
// 結果はログに "[repl]" を付けて出す (SIV3D_LOG_FILE と組み合わせて読む)。書き込み途中のファイルを読まないよう、
// 別名で書いてから名前を変えて置くこと。詳しくは SDL3/DEBUG.md。
//
//   capture <パス>          画面を PNG などで保存する (このフレームの描画の後)
//   move <x> <y>            マウスカーソルを動かす (シーンの座標)
//   click <x> <y> [l|r|m]   その位置をクリックする (押して離す。1 フレーム以内でも取りこぼさない)
//   key <キー名>            キーを押して離す (SDL のキー名: A / Space / Return / Escape / F1 / Left など)
//   text <文字列>           文字の入力 (IME の確定) を送る (SDL_EVENT_TEXT_INPUT。日本語も可)
//   editing [文字列]        IME の変換中の文字を送る (SDL_EVENT_TEXT_EDITING。空で変換中を消す)
//   info                    シーン・ウィンドウの大きさ、FPS、カーソルの位置、文字入力の受付中かを出す
//   log <文字列>            ログに目印を書く
//   quit                    アプリを終了する (System::Exit)
//   # ...                   コメント
//

# include <cstdlib>
# include <deque>
# include <filesystem>
# include <fstream>
# include <sstream>
# include <string>
# include <system_error>
# include <Siv3D/Common.hpp>
# include <Siv3D/EngineLog.hpp>
# include <Siv3D/FormatLiteral.hpp>
# include <Siv3D/Unicode.hpp>
# include <Siv3D/Scene.hpp>
# include <Siv3D/Window.hpp>
# include <Siv3D/Cursor.hpp>
# include <Siv3D/Profiler.hpp>
# include <Siv3D/ScreenCapture.hpp>
# include <Siv3D/System.hpp>
# include <Siv3D/Renderer/IRenderer.hpp>
# include <Siv3D/Common/Siv3DEngine.hpp>
# include <SDL3/SDL.h>
# if SIV3D_PLATFORM(WINDOWS)
#	include <Siv3D/Windows/Windows.hpp>
# endif

namespace s3d::detail
{
	// Cursor/CCursor.cpp: REPL が指定したカーソルの位置 (実際のマウスが動くまで使う)
	void SetReplCursorPos(const Vec2& windowPos);

	namespace
	{
		[[nodiscard]]
		std::filesystem::path GetReplFilePath()
		{
		# if SIV3D_PLATFORM(WINDOWS)
			if (const wchar_t* path = ::_wgetenv(L"SIV3D_REPL_FILE"); path && *path)
			{
				return std::filesystem::path{ path };
			}
		# else
			if (const char* path = std::getenv("SIV3D_REPL_FILE"); path && *path)
			{
				return std::filesystem::path{ path };
			}
		# endif
			return{};
		}

		// シーンの座標 → ウィンドウの座標 (SDL のポイント)。CCursor の変換 (レターボックス・ピクセル密度) の逆
		[[nodiscard]]
		Vec2 SceneToWindow(SDL_Window* window, const Vec2& scenePos)
		{
			const auto [s, viewRect] = SIV3D_ENGINE(Renderer)->getLetterboxComposition();
			const Vec2 pixelPos = (scenePos * s + viewRect.pos);
			float density = SDL_GetWindowPixelDensity(window);
			if (density <= 0.0f) { density = 1.0f; }
			return (pixelPos / density);
		}

		void PushMouseButton(SDL_Window* window, const Vec2& windowPos, const Uint8 button, const bool down)
		{
			SDL_Event event{};
			event.type = (down ? SDL_EVENT_MOUSE_BUTTON_DOWN : SDL_EVENT_MOUSE_BUTTON_UP);
			event.button.windowID = SDL_GetWindowID(window);
			event.button.button = button;
			event.button.down = down;
			event.button.clicks = 1;
			event.button.x = static_cast<float>(windowPos.x);
			event.button.y = static_cast<float>(windowPos.y);
			SDL_PushEvent(&event);
		}

		void PushKey(SDL_Window* window, const SDL_Scancode scancode, const bool down)
		{
			SDL_Event event{};
			event.type = (down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP);
			event.key.windowID = SDL_GetWindowID(window);
			event.key.scancode = scancode;
			event.key.key = SDL_GetKeyFromScancode(scancode, SDL_KMOD_NONE, false);
			event.key.down = down;
			event.key.repeat = false;
			SDL_PushEvent(&event);
		}

		// SDL_PushEvent は文字列を複製しないので、イベントが処理されるまで文字列を持っておく (直近の数件)
		const char* KeepText(const std::string& text)
		{
			static std::deque<std::string> texts;
			texts.push_back(text);
			while (16 < texts.size()) { texts.pop_front(); }
			return texts.back().c_str();
		}

		void Execute(SDL_Window* window, const std::string& line)
		{
			std::istringstream iss{ line };
			std::string command;
			iss >> command;

			if (command.empty() || (command[0] == '#'))
			{
				return;
			}

			const String commandLine = Unicode::FromUTF8(line);
			LOG_INFO(U"[repl] > {}"_fmt(commandLine));

			if (command == "capture")
			{
				std::string rest;
				std::getline(iss >> std::ws, rest);
				const String path = Unicode::FromUTF8(rest);
				ScreenCapture::SaveCurrentFrame(path.isEmpty() ? U"capture.png" : path);
				LOG_INFO(U"[repl] capture requested: {}"_fmt(path));
			}
			else if ((command == "move") || (command == "click"))
			{
				double x = 0, y = 0;
				std::string buttonName = "l";

				if (not (iss >> x >> y))
				{
					LOG_INFO(U"[repl] error: usage: {} <x> <y>"_fmt(Unicode::FromUTF8(command)));
					return;
				}

				iss >> buttonName;
				const Vec2 windowPos = SceneToWindow(window, Vec2{ x, y });
				SDL_WarpMouseInWindow(window, static_cast<float>(windowPos.x), static_cast<float>(windowPos.y));
				// Wayland などでは warp が無視されることがあるので、カーソルの位置としても指定する
				SetReplCursorPos(windowPos);

				if (command == "click")
				{
					const Uint8 button = (buttonName == "r") ? SDL_BUTTON_RIGHT : (buttonName == "m") ? SDL_BUTTON_MIDDLE : SDL_BUTTON_LEFT;
					PushMouseButton(window, windowPos, button, true);
					PushMouseButton(window, windowPos, button, false);
				}

				LOG_INFO(U"[repl] {} ({}, {}) -> window ({:.1f}, {:.1f})"_fmt(Unicode::FromUTF8(command), x, y, windowPos.x, windowPos.y));
			}
			else if (command == "key")
			{
				std::string name;
				std::getline(iss >> std::ws, name);
				const SDL_Scancode scancode = SDL_GetScancodeFromName(name.c_str());

				if (scancode == SDL_SCANCODE_UNKNOWN)
				{
					LOG_INFO(U"[repl] error: unknown key name: {}"_fmt(Unicode::FromUTF8(name)));
					return;
				}

				PushKey(window, scancode, true);
				PushKey(window, scancode, false);
				LOG_INFO(U"[repl] key {} (scancode {})"_fmt(Unicode::FromUTF8(name), static_cast<int32>(scancode)));
			}
			else if ((command == "text") || (command == "editing"))
			{
				std::string text;
				std::getline(iss >> std::ws, text);

				SDL_Event event{};
				if (command == "text")
				{
					if (text.empty())
					{
						LOG_INFO(U"[repl] error: usage: text <string>");
						return;
					}
					event.type = SDL_EVENT_TEXT_INPUT;
					event.text.windowID = SDL_GetWindowID(window);
					event.text.text = KeepText(text);
				}
				else
				{
					event.type = SDL_EVENT_TEXT_EDITING;
					event.edit.windowID = SDL_GetWindowID(window);
					event.edit.text = KeepText(text);
					event.edit.start = static_cast<Sint32>(Unicode::FromUTF8(text).size());
					event.edit.length = 0;
				}
				SDL_PushEvent(&event);
				LOG_INFO(U"[repl] {} \"{}\""_fmt(Unicode::FromUTF8(command), Unicode::FromUTF8(text)));
			}
			else if (command == "info")
			{
				LOG_INFO(U"[repl] scene {} / window client {} / frame buffer {} / fps {} / cursor {} / text input {}"_fmt(
					Scene::Size(), Window::GetState().virtualSize, Window::GetState().frameBufferSize, Profiler::FPS(), Cursor::PosF(),
					(SDL_TextInputActive(window) ? U"active" : U"inactive")));
			}
			else if (command == "log")
			{
				std::string rest;
				std::getline(iss >> std::ws, rest);
				LOG_INFO(U"[repl] {}"_fmt(Unicode::FromUTF8(rest)));
			}
			else if (command == "quit")
			{
				LOG_INFO(U"[repl] quit");
				System::Exit();
			}
			else
			{
				LOG_INFO(U"[repl] error: unknown command: {}"_fmt(Unicode::FromUTF8(command)));
			}
		}
	}

	// CWindow::update の先頭 (SDL のイベントを処理する前) から毎フレーム呼ぶ。押したイベントはこのフレームで処理される
	void PollRepl(SDL_Window* window)
	{
		static const std::filesystem::path path = GetReplFilePath();

		if (path.empty())
		{
			return;
		}

		std::error_code ec;

		if (not std::filesystem::is_regular_file(path, ec))
		{
			return;
		}

		std::string content;
		{
			std::ifstream ifs{ path, std::ios::binary };
			std::ostringstream oss;
			oss << ifs.rdbuf();
			content = oss.str();
		}

		std::filesystem::remove(path, ec);

		std::istringstream lines{ content };
		std::string line;

		while (std::getline(lines, line))
		{
			if ((not line.empty()) && (line.back() == '\r'))
			{
				line.pop_back();
			}

			Execute(window, line);
		}
	}
}
