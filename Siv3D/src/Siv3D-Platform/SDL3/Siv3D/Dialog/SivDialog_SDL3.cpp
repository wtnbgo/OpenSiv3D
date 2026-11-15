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

# if SIV3D_PLATFORM(LINUX)
#	include <csignal>
#	include <fstream>
#	include <filesystem>
#	include <unistd.h>
# endif

//
// SDL3 のファイルダイアログ (SDL_ShowFileDialogWithProperties) による実装。
//   Windows: コモンダイアログ / macOS: NSOpenPanel・NSSavePanel /
//   Linux: xdg-desktop-portal (D-Bus)、無ければ zenity
// SDL3 の API は非同期 (コールバック) なので、結果が返るまで SDL_PumpEvents を回して待つ。
// 汲み上げたイベントはキューに残るため、Siv3D のメインループ側で通常どおり処理される。
//

namespace s3d
{
# if SIV3D_PLATFORM(MACOS) && !SIV3D_PLATFORM(IOS)

	// SivDialog_SDL3_macOS.mm
	void MacOS_BeginFileDialog();
	void MacOS_EndFileDialog();
	void MacOS_CancelFileDialogs();

# endif

	namespace
	{
		// 終了要求 (SDL_EVENT_QUIT) を受け取ったか。
		// Siv3D は QUIT を処理したフレームの System::Update ではまだ true を返す (終了するのは次の Update) ため、
		// その間に開かれたダイアログは表示せずにキャンセル扱いで返す
		// (QUIT はすでにキューから取り出されているので、表示すると終了要求を検出できず固まる)。
		bool g_quitRequested = false;

	# if SIV3D_PLATFORM(LINUX)

		// zenity フォールバック時、SDL3 は zenity を自プロセスの子として起動し、閉じる API を持たない。
		// 終了要求で待機を打ち切ってもダイアログ (zenity) は残り、アプリ終了後も画面に出たままになるため、
		// 自プロセスを親に持つ zenity にだけ SIGKILL を送って閉じる (portal の場合は D-Bus の切断で portal 側が閉じる)。
		// SDL3 はシグナルをブロックしたスレッドから zenity を起動するため、子は SIGTERM / SIGINT / SIGHUP を
		// ブロックした状態を引き継いでおり、SIGTERM では終了しない (ファイル選択だけなので強制終了で問題ない)
		void CloseChildZenityDialogs()
		{
			const pid_t self = ::getpid();
			std::error_code ec;

			for (const auto& entry : std::filesystem::directory_iterator{ "/proc", ec })
			{
				const std::string name = entry.path().filename().string();

				if (name.empty() || (name.find_first_not_of("0123456789") != std::string::npos))
				{
					continue;
				}

				// /proc/<pid>/stat: "<pid> (<comm>) <state> <ppid> ..."
				std::ifstream ifs{ entry.path() / "stat" };
				std::string stat;
				std::getline(ifs, stat);

				const size_t open = stat.find('(');
				const size_t close = stat.rfind(')');

				if ((open == std::string::npos) || (close == std::string::npos) || (close < open))
				{
					continue;
				}

				const std::string comm = stat.substr(open + 1, (close - open - 1));
				char state = 0;
				long ppid = 0;

				if ((comm != "zenity") || (std::sscanf(stat.c_str() + close + 1, " %c %ld", &state, &ppid) != 2) || (ppid != self))
				{
					continue;
				}

				LOG_INFO(U"File dialog: closing zenity (pid {})"_fmt(Unicode::Widen(name)));
				::kill(static_cast<pid_t>(std::stol(name)), SIGKILL);
			}
		}

	# endif

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

		struct DialogResult
		{
			std::atomic<bool> done{ false };

			bool error = false;

			Array<FilePath> paths;
		};

		// SDL に渡すデータ一式。filters / location 等はコールバックが呼ばれるまで有効である必要があるので、
		// 終了要求で待機を打ち切った場合 (Windows / Linux) は解放せずに残す
		struct DialogRequest
		{
			std::vector<std::string> names;

			std::vector<std::string> patterns;

			std::vector<SDL_DialogFileFilter> filters;

			std::string location;

			std::string title;

			SDL_PropertiesID props = 0;

			DialogResult result;
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
			// 終了要求を処理済みのフレームで開かれたダイアログは表示しない
			if (g_quitRequested || (QuitTerminatesApp() && HasQuitEvent()))
			{
				g_quitRequested = true;
				LOG_INFO(U"File dialog: skipped (quit requested)");
				return{};
			}

			DialogRequest* request = new DialogRequest;

			// SDL のフィルタ書式: name と、セミコロン区切りの拡張子 ("png;jpg")。"*" はすべて
			request->names.reserve(filters.size());
			request->patterns.reserve(filters.size());

			for (const auto& filter : filters)
			{
				request->names.push_back(filter.name.narrow());
				request->patterns.push_back(filter.patterns.isEmpty() ? std::string{ "*" } : filter.patterns.join(U";", U"", U"").narrow());
			}

			request->filters.reserve(filters.size());

			for (size_t i = 0; i < request->names.size(); ++i)
			{
				request->filters.push_back(SDL_DialogFileFilter{ request->names[i].c_str(), request->patterns[i].c_str() });
			}

			// ディレクトリ指定の末尾 '/' は呼び出し側 (AsDirectoryLocation) で付ける
			request->location = ToNativePath(defaultLocation);
			request->title = title.narrow();

			SDL_Window* window = static_cast<SDL_Window*>(SIV3D_ENGINE(Window)->getHandle());

			const SDL_PropertiesID props = request->props = SDL_CreateProperties();

			if (not request->filters.empty())
			{
				SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, request->filters.data());
				SDL_SetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER, static_cast<Sint64>(request->filters.size()));
			}

			if (window)
			{
				SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, window);
			}

			if (not request->location.empty())
			{
				SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_LOCATION_STRING, request->location.c_str());
			}

			if (not request->title.empty())
			{
				SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING, request->title.c_str());
			}

			SDL_SetBooleanProperty(props, SDL_PROP_FILE_DIALOG_MANY_BOOLEAN, allowMany);

		# if SIV3D_PLATFORM(MACOS) && !SIV3D_PLATFORM(IOS)
			// ダイアログ表示中だけ Edit メニューを追加する (パネル内で ⌘A / ⌘C / ⌘V 等を効かせるため)
			MacOS_BeginFileDialog();
		# endif

			DialogResult& result = request->result;

			SDL_ShowFileDialogWithProperties(type, OnDialogResult, &result, props);

			bool cancelRequested = false;

			while (not result.done.load(std::memory_order_acquire))
			{
				SDL_PumpEvents();

				// 終了要求 (SIGTERM / ⌘Q など) が来たらダイアログをキャンセルして戻る。
				// SDL_EVENT_QUIT はキューから取り出さずに残し、Siv3D のメインループ (System::Update) で通常どおり処理させる。
				if ((not cancelRequested) && HasQuitEvent())
				{
					cancelRequested = true;
					g_quitRequested = QuitTerminatesApp();

				# if SIV3D_PLATFORM(MACOS) && !SIV3D_PLATFORM(IOS)
					// 表示中のパネルに cancel: を送ると、SDL のコールバックがキャンセルで呼ばれる
					LOG_INFO(U"File dialog: cancelled by quit request");
					MacOS_CancelFileDialogs();
				# else
					// Windows / Linux の SDL3 には表示中のダイアログを閉じる手段が無いので、
					// アプリが終了する場合に限って待機を打ち切る。コールバックは後から呼ばれうるため request は解放しない
					// (このあとアプリは終了する)
					if (g_quitRequested)
					{
						LOG_INFO(U"File dialog: abandoned by quit request");
					# if SIV3D_PLATFORM(LINUX)
						CloseChildZenityDialogs();
					# endif
						return{};
					}
				# endif
				}

				SDL_Delay(10);
			}

		# if SIV3D_PLATFORM(MACOS) && !SIV3D_PLATFORM(IOS)
			MacOS_EndFileDialog();
		# endif

			// Siv3D のパス表記 ('/' 区切り・絶対パス) にそろえる
			Array<FilePath> paths = result.paths.map([](const FilePath& path) { return FileSystem::FullPath(path); });

			SDL_DestroyProperties(props);
			delete request;

			return paths;
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
