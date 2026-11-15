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

# include <atomic>
# include <string>
# include <vector>
# include <SDL3/SDL.h>
# include <Siv3D/Dialog.hpp>
# include <Siv3D/Unicode.hpp>
# include <Siv3D/FileSystem.hpp>
# include <Siv3D/EngineLog.hpp>
# include <Siv3D/Window/IWindow.hpp>
# include <Siv3D/UserAction.hpp>
# include <Siv3D/UserAction/IUserAction.hpp>
# include <Siv3D/Common/Siv3DEngine.hpp>

//
// SDL3 のファイルダイアログ (SDL_ShowFileDialogWithProperties) による実装。
//   Windows: コモンダイアログ / macOS: NSOpenPanel・NSSavePanel /
//   Linux: xdg-desktop-portal (D-Bus)、無ければ zenity
// SDL3 の API は非同期 (コールバック) なので、結果が返るまで SDL_PumpEvents を回して待つ。
// 汲み上げたイベントはキューに残るため、Siv3D のメインループ側で通常どおり処理される。
//

namespace s3d
{
# if SIV3D_PLATFORM(MACOS)

	// SivDialog_SDL3_macOS.mm
	void MacOS_BeginFileDialog();
	void MacOS_EndFileDialog();
	void MacOS_CancelFileDialogs();

	namespace
	{
		// 終了要求 (SDL_EVENT_QUIT) を受け取ったか。
		// Siv3D は QUIT を処理したフレームの System::Update ではまだ true を返す (終了するのは次の Update) ため、
		// その間に開かれたダイアログは表示せずにキャンセル扱いで返す
		// (QUIT はすでにキューから取り出されているので、表示すると終了要求を検出できず固まる)。
		bool g_quitRequested = false;

		[[nodiscard]]
		bool HasQuitEvent()
		{
			SDL_Event event;
			return (SDL_PeepEvents(&event, 1, SDL_PEEKEVENT, SDL_EVENT_QUIT, SDL_EVENT_QUIT) > 0);
		}

		// 終了要求でアプリが終了する設定か (System::SetTerminationTriggers で外されていれば終了しないので、以降のダイアログは止めない)
		[[nodiscard]]
		bool QuitTerminatesApp()
		{
			return ((SIV3D_ENGINE(UserAction)->getTerminationTriggers() & UserAction::CloseButtonClicked) != 0);
		}
	}

# endif

	namespace
	{
		struct DialogResult
		{
			std::atomic<bool> done{ false };

			bool error = false;

			Array<FilePath> paths;
		};

		void SDLCALL OnDialogResult(void* userdata, const char* const* filelist, int)
		{
			DialogResult* result = static_cast<DialogResult*>(userdata);

			if (filelist == nullptr)
			{
				result->error = true;
				LOG_ERROR(U"File dialog failed: {}"_fmt(Unicode::Widen(SDL_GetError())));
			}
			else
			{
				// 空のリスト = キャンセル
				for (const char* const* p = filelist; *p; ++p)
				{
					result->paths << Unicode::Widen(*p);
				}
			}

			result->done.store(true, std::memory_order_release);
		}

		[[nodiscard]]
		std::string ToNativePath(const FilePathView path)
		{
			std::string s = path.narrow();
		# if SIV3D_PLATFORM(WINDOWS)
			for (char& c : s)
			{
				if (c == '/')
				{
					c = '\\';
				}
			}
		# endif
			return s;
		}

		// SDL3 はファイル用ダイアログの location を「ファイルのパス」として扱い、末尾が '/' で
		// なければ最後の要素をファイル名、その親を初期フォルダにする (portal / zenity とも)。
		// Siv3D の defaultPath は初期ディレクトリなので、末尾に '/' を付けて渡す
		[[nodiscard]]
		FilePath AsDirectoryLocation(const FilePathView path)
		{
			FilePath location{ path };

			if (location && (not location.ends_with(U'/')) && (not location.ends_with(U'\\')))
			{
				location += U'/';
			}

			return location;
		}

		[[nodiscard]]
		Array<FilePath> ShowDialog(const SDL_FileDialogType type, const Array<FileFilter>& filters,
			const FilePathView defaultLocation, const StringView title, const bool allowMany)
		{
			// SDL のフィルタ書式: name と、セミコロン区切りの拡張子 ("png;jpg")。"*" はすべて
			std::vector<std::string> names, patterns;
			names.reserve(filters.size());
			patterns.reserve(filters.size());

			for (const auto& filter : filters)
			{
				names.push_back(filter.name.narrow());
				patterns.push_back(filter.patterns.isEmpty() ? std::string{ "*" } : filter.patterns.join(U";", U"", U"").narrow());
			}

			// filters はコールバックが呼ばれるまで有効である必要がある (この関数内で待つので OK)
			std::vector<SDL_DialogFileFilter> sdlFilters;
			sdlFilters.reserve(filters.size());

			for (size_t i = 0; i < names.size(); ++i)
			{
				sdlFilters.push_back(SDL_DialogFileFilter{ names[i].c_str(), patterns[i].c_str() });
			}

			// ディレクトリ指定の末尾 '/' は呼び出し側 (AsDirectoryLocation) で付ける
			const std::string location = ToNativePath(defaultLocation);
			const std::string titleUTF8 = title.narrow();

			SDL_Window* window = static_cast<SDL_Window*>(SIV3D_ENGINE(Window)->getHandle());

			const SDL_PropertiesID props = SDL_CreateProperties();

			if (not sdlFilters.empty())
			{
				SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, sdlFilters.data());
				SDL_SetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, static_cast<Sint64>(sdlFilters.size()));
			}

			if (window)
			{
				SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, window);
			}

			if (not location.empty())
			{
				SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_LOCATION_STRING, location.c_str());
			}

			if (not titleUTF8.empty())
			{
				SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING, titleUTF8.c_str());
			}

			SDL_SetBooleanProperty(props, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, allowMany);

			DialogResult result;

		# if SIV3D_PLATFORM(MACOS)
			if (g_quitRequested || (QuitTerminatesApp() && HasQuitEvent()))
			{
				g_quitRequested = true;
				SDL_DestroyProperties(props);
				LOG_INFO(U"File dialog: skipped (quit requested)");
				return{};
			}

			// ダイアログ表示中だけ Edit メニューを追加する (パネル内で ⌘A / ⌘C / ⌘V 等を効かせるため)
			MacOS_BeginFileDialog();
		# endif

			SDL_ShowFileDialogWithProperties(type, OnDialogResult, &result, props);

		# if SIV3D_PLATFORM(MACOS)
			bool cancelRequested = false;
		# endif

			while (not result.done.load(std::memory_order_acquire))
			{
				SDL_PumpEvents();

			# if SIV3D_PLATFORM(MACOS)
				// 終了要求 (SIGTERM / ⌘Q など) が来たらダイアログをキャンセルして戻る。
				// SDL_EVENT_QUIT はキューから取り出さずに残し、Siv3D のメインループ (System::Update) で通常どおり処理させる。
				if ((not cancelRequested) && HasQuitEvent())
				{
					LOG_INFO(U"File dialog: cancelled by quit request");
					MacOS_CancelFileDialogs();
					cancelRequested = true;
					g_quitRequested = QuitTerminatesApp();
				}
			# endif

				SDL_Delay(10);
			}

		# if SIV3D_PLATFORM(MACOS)
			MacOS_EndFileDialog();
		# endif

			SDL_DestroyProperties(props);

			// Siv3D のパス表記 ('/' 区切り・絶対パス) にそろえる
			return result.paths.map([](const FilePath& path) { return FileSystem::FullPath(path); });
		}
	}

	namespace Dialog
	{
		Optional<FilePath> OpenFile(const Array<FileFilter>& filters, const FilePathView defaultPath, const StringView title)
		{
			const Array<FilePath> paths = ShowDialog(SDL_FILEDIALOG_OPENFILE, filters, AsDirectoryLocation(defaultPath), title, false);

			if (paths.isEmpty())
			{
				return none;
			}

			return paths.front();
		}

		Array<FilePath> OpenFiles(const Array<FileFilter>& filters, const FilePathView defaultPath, const StringView title)
		{
			return ShowDialog(SDL_FILEDIALOG_OPENFILE, filters, AsDirectoryLocation(defaultPath), title, true);
		}

		Optional<FilePath> SaveFile(const Array<FileFilter>& filters, const FilePathView defaultPath, const StringView title, const StringView defaultFileName)
		{
			// 保存ダイアログの初期位置にはファイル名まで含めたパスを渡せる
			FilePath location = AsDirectoryLocation(defaultPath);

			if (defaultFileName)
			{
				if (location && (not location.ends_with(U'/')) && (not location.ends_with(U'\\')))
				{
					location += U'/';
				}

				location += defaultFileName;
			}

			const Array<FilePath> paths = ShowDialog(SDL_FILEDIALOG_SAVEFILE, filters, location, title, false);

			if (paths.isEmpty())
			{
				return none;
			}

			return paths.front();
		}

		Optional<FilePath> SelectFolder(const FilePathView defaultPath, const StringView title)
		{
			const Array<FilePath> paths = ShowDialog(SDL_FILEDIALOG_OPENFOLDER, {}, AsDirectoryLocation(defaultPath), title, false);

			if (paths.isEmpty())
			{
				return none;
			}

			return paths.front();
		}
	}
}
