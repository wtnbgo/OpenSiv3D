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

# include <Siv3D/Platform.hpp>
# if SIV3D_PLATFORM(WINDOWS)
#	include <Siv3D/Windows/Windows.hpp>
#	include <Shlobj.h>
#	include <direct.h>
# else
#	include <sys/stat.h>
#	include <unistd.h>
#	include <fcntl.h>
#	include <cerrno>
#	include <ctime>
#	include <cstdio>
#	include <cstdlib>
#	include <string>
# endif
# if SIV3D_PLATFORM(LINUX)
#	include <sys/sysmacros.h>
#	include <fstream>
#	include <sstream>
#	include <array>
#	include <string_view>
# endif
# if SIV3D_PLATFORM(MACOS)
#	include <mach-o/dyld.h>
#	include <climits>
#	include <cstdlib>
# endif
# include <filesystem>
# include <Siv3D/String.hpp>
# include <Siv3D/FileSystem.hpp>
# include <Siv3D/EnvironmentVariable.hpp>
# include <Siv3D/INI.hpp>
# include <Siv3D/Unicode.hpp>

namespace s3d
{
	namespace fs = std::filesystem;

	namespace detail
	{
		[[nodiscard]]
		static fs::path ToPath(const FilePathView path)
		{
		# if SIV3D_PLATFORM(WINDOWS)
			return fs::path(Unicode::ToWstring(path));
		# else
			return fs::path(path.toUTF8());
		# endif
		}

		[[nodiscard]]
		static fs::file_status GetStatus(const FilePathView path)
		{
			return fs::status(detail::ToPath(path));
		}

	# if SIV3D_PLATFORM(WINDOWS)

		[[nodiscard]]
		static bool Exists(const FilePathView path)
		{
			return fs::exists(detail::ToPath(path));
		}

		[[nodiscard]]
		static bool IsRegular(const FilePathView path)
		{
			return fs::is_regular_file(detail::ToPath(path));
		}

		[[nodiscard]]
		static bool IsDirectory(const FilePathView path)
		{
			return fs::is_directory(detail::ToPath(path));
		}

	# else

		[[nodiscard]]
		static bool GetStat(const FilePathView path, struct stat& s)
		{
			return (::stat(FilePath(path).replaced(U'\\', U'/').narrow().c_str(), &s) == 0);
		}	

		[[nodiscard]]
		static bool Exists(const FilePathView path)
		{
			struct stat s;
			return GetStat(path, s);
		}

		[[nodiscard]]
		static bool IsRegular(const FilePathView path)
		{
			struct stat s;
			if (!GetStat(path, s))
			{
				return false;
			}

			return S_ISREG(s.st_mode);
		}

		[[nodiscard]]
		static bool IsDirectory(const FilePathView path)
		{
			struct stat s;
			if (!GetStat(path, s))
			{
				return false;
			}

			return S_ISDIR(s.st_mode);
		}

	# endif

		[[nodiscard]]
		static FilePath NormalizePath(FilePath path, const bool skipDirectoryCheck = false)
		{
			path.replace(U'\\', U'/');

			if (!path.ends_with(U'/')
				&& (skipDirectoryCheck || (GetStatus(path).type() == fs::file_type::directory)))
			{
				path.push_back(U'/');
			}

			return path;
		}

	# if SIV3D_PLATFORM(WINDOWS)

		[[nodiscard]]
		static DateTime ToDateTime(const FILETIME& ft)
		{
			SYSTEMTIME st;
			::FileTimeToLocalFileTime(&ft, const_cast<FILETIME*>(&ft));
			::FileTimeToSystemTime(&ft, &st);
			return{ st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond, st.wMilliseconds };
		}

	# else

		[[nodiscard]]
		static DateTime ToDateTime(const ::timespec& tv)
		{
			::tm lt;
			::localtime_r(&tv.tv_sec, &lt);
			return{ (1900 + lt.tm_year),
					(1 + lt.tm_mon),
					(lt.tm_mday),
					lt.tm_hour,
					lt.tm_min,
					lt.tm_sec,
					static_cast<int32>(tv.tv_nsec / (1'000'000))};
		}

	# endif

		[[nodiscard]]
		inline constexpr std::filesystem::copy_options ToCopyOptions(const CopyOption copyOption) noexcept
		{
			switch (copyOption)
			{
			case CopyOption::SkipExisting:
				return fs::copy_options::skip_existing;
			case CopyOption::OverwriteExisting:
				return fs::copy_options::overwrite_existing;
			case CopyOption::UpdateExisting:
				return fs::copy_options::update_existing;
			default:
				return fs::copy_options::none;
			}
		}

	# if SIV3D_PLATFORM(WINDOWS)

		[[nodiscard]]
		static bool TrashFile(const FilePathView path)
		{
			const std::wstring wpath = Unicode::ToWstring(path);
			std::wstring from(wpath);
			from.push_back(L'\0'); // double null-terminated

			SHFILEOPSTRUCTW fileOp = {};
			fileOp.wFunc = FO_DELETE;
			fileOp.pFrom = from.c_str();
			fileOp.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_NOERRORUI | FOF_SILENT;

			return (::SHFileOperationW(&fileOp) == 0 && !fileOp.fAnyOperationsAborted);
		}

	# elif SIV3D_PLATFORM(MACOS)

	} // namespace detail

	// SivFileSystem_SDL3_macOS.mm (NSFileManager trashItemAtURL)
	bool MacOS_TrashFile(const char* path);

	namespace detail
	{
		[[nodiscard]]
		static bool TrashFile(const FilePathView path)
		{
			return MacOS_TrashFile(path.narrow().c_str());
		}

	# elif SIV3D_PLATFORM(LINUX)

		//
		// freedesktop.org Trash 仕様 (https://specifications.freedesktop.org/trash-spec/) の実装。
		// GIO の g_file_trash と同じ配置にするので、各デスクトップのゴミ箱から見え、元に戻せる。
		//   - ホームと同じファイルシステム: $XDG_DATA_HOME/Trash (既定 ~/.local/share/Trash)
		//   - 別のファイルシステム: <マウントポイント>/.Trash-<uid>
		//   - files/<名前> に本体、info/<名前>.trashinfo に元のパスと削除日時
		//

		[[nodiscard]]
		static std::string TrashURLEncode(const std::string& s)
		{
			static constexpr char Hex[] = "0123456789ABCDEF";
			std::string out;
			out.reserve(s.size());

			for (const unsigned char c : s)
			{
				if (('A' <= c && c <= 'Z') || ('a' <= c && c <= 'z') || ('0' <= c && c <= '9')
					|| (c == '-') || (c == '_') || (c == '.') || (c == '~') || (c == '/'))
				{
					out.push_back(static_cast<char>(c));
				}
				else
				{
					out.push_back('%');
					out.push_back(Hex[c >> 4]);
					out.push_back(Hex[c & 0xF]);
				}
			}

			return out;
		}

		[[nodiscard]]
		static bool MakeDirectory700(const std::string& path)
		{
			std::error_code ec;
			fs::create_directories(path, ec);
			if (ec)
			{
				return false;
			}
			::chmod(path.c_str(), 0700);
			return true;
		}

		// path と同じデバイス上で、最も上位のディレクトリ (= マウントポイント) を返す
		[[nodiscard]]
		static std::string FindMountPoint(const std::string& path, const dev_t dev)
		{
			fs::path current = fs::path(path).parent_path();

			while (current.has_parent_path() && (current != current.parent_path()))
			{
				struct stat st;
				if ((::stat(current.parent_path().c_str(), &st) != 0) || (st.st_dev != dev))
				{
					break;
				}
				current = current.parent_path();
			}

			return current.string();
		}

		// /proc/self/mountinfo のマウントポイントは空白などが \ooo (8 進) でエスケープされている
		[[nodiscard]]
		static std::string UnescapeMountInfo(const std::string& s)
		{
			std::string out;
			out.reserve(s.size());

			for (size_t i = 0; i < s.size(); ++i)
			{
				if ((s[i] == '\\') && (i + 3 < s.size() + 0) && ('0' <= s[i + 1] && s[i + 1] <= '7')
					&& ('0' <= s[i + 2] && s[i + 2] <= '7') && ('0' <= s[i + 3] && s[i + 3] <= '7'))
				{
					out.push_back(static_cast<char>(((s[i + 1] - '0') << 6) | ((s[i + 2] - '0') << 3) | (s[i + 3] - '0')));
					i += 3;
				}
				else
				{
					out.push_back(s[i]);
				}
			}

			return out;
		}

		// dev のファイルシステムが「システム内部のマウント」か。
		// GIO (g_file_trash) はシステム内部のマウント (tmpfs の /tmp など) ではゴミ箱への移動を
		// サポートせず失敗を返す。そこに .Trash-<uid> を作っても GNOME などのゴミ箱 (GVfs) には表示されず、
		// 元に戻せない (tmpfs なら再起動で消える) ため、同じ判定で失敗させる。
		// 判定は GIO の g_unix_is_system_fs_type / g_unix_is_mount_path_system_internal に準じる
		[[nodiscard]]
		static bool IsSystemInternalMount(const dev_t dev)
		{
			static constexpr std::array<std::string_view, 37> SystemFsTypes =
			{
				"adfs", "afs", "auto", "autofs", "autofs4", "binfmt_misc", "bpf", "cgroup", "cgroup2",
				"configfs", "debugfs", "devfs", "devpts", "devtmpfs", "efivarfs", "fusectl", "hugetlbfs",
				"mqueue", "nfsd", "nsfs", "overlay", "proc", "procfs", "pstore", "ramfs", "rootfs",
				"rpc_pipefs", "securityfs", "selinuxfs", "squashfs", "sysfs", "tmpfs", "tracefs", "usbfs",
				"fuse.portal", "fuse.gvfsd-fuse", "fuse.snapfuse",
			};

			static constexpr std::array<std::string_view, 25> SystemMountPaths =
			{
				"/", "/bin", "/boot", "/compat/linux/proc", "/compat/linux/sys", "/dev", "/etc", "/home",
				"/lib", "/lib64", "/libexec", "/live/cow", "/live/image", "/media", "/mnt", "/opt",
				"/proc", "/run", "/sbin", "/srv", "/sys", "/tmp", "/usr", "/var", "/var/tmp",
			};

			std::ifstream ifs{ "/proc/self/mountinfo" };
			std::string line;

			while (std::getline(ifs, line))
			{
				// <id> <parent> <major>:<minor> <root> <mountpoint> <options> [optional...] - <fstype> <source> <superopts>
				std::istringstream iss{ line };
				std::string id, parent, majmin, root, mountPoint;

				if (not (iss >> id >> parent >> majmin >> root >> mountPoint))
				{
					continue;
				}

				unsigned int major = 0, minor = 0;

				if ((std::sscanf(majmin.c_str(), "%u:%u", &major, &minor) != 2) || (makedev(major, minor) != dev))
				{
					continue;
				}

				const size_t sep = line.find(" - ");
				std::string fsType;

				if (sep != std::string::npos)
				{
					std::istringstream rest{ line.substr(sep + 3) };
					rest >> fsType;
				}

				const std::string mp = UnescapeMountInfo(mountPoint);

				for (const auto& t : SystemFsTypes)
				{
					if (fsType == t)
					{
						return true;
					}
				}

				for (const auto& m : SystemMountPaths)
				{
					if (mp == m)
					{
						return true;
					}
				}

				// /var/... 以下 (/var/lib/docker など) もシステム内部扱い
				if (mp.starts_with("/var/") || mp.starts_with("/proc/") || mp.starts_with("/sys/") || mp.starts_with("/dev/"))
				{
					return true;
				}

				return false;
			}

			// 見つからない場合は従来どおり (ゴミ箱に入れる)
			return false;
		}

		[[nodiscard]]
		static bool TrashFile(const FilePathView path)
		{
			std::string src = fs::absolute(ToPath(path)).lexically_normal().string();

			while ((1 < src.size()) && src.ends_with('/'))
			{
				src.pop_back();
			}

			struct stat srcStat;
			if (::lstat(src.c_str(), &srcStat) != 0)
			{
				return false;
			}

			// ホームのゴミ箱
			std::string homeTrash;
			if (const char* xdg = std::getenv("XDG_DATA_HOME"); xdg && *xdg)
			{
				homeTrash = std::string{ xdg } + "/Trash";
			}
			else if (const char* home = std::getenv("HOME"); home && *home)
			{
				homeTrash = std::string{ home } + "/.local/share/Trash";
			}
			else
			{
				return false;
			}

			if (not MakeDirectory700(homeTrash))
			{
				return false;
			}

			std::string trashDir;
			std::string infoPath; // .trashinfo の Path= に書く値

			struct stat trashStat;
			if ((::stat(homeTrash.c_str(), &trashStat) == 0) && (trashStat.st_dev == srcStat.st_dev))
			{
				trashDir = homeTrash;
				infoPath = src; // 絶対パス
			}
			else
			{
				// 別のファイルシステム: <topdir>/.Trash-<uid> (Path= は topdir からの相対)。
				// ただしシステム内部のマウント (tmpfs の /tmp など) では GIO と同じく失敗を返す
				if (IsSystemInternalMount(srcStat.st_dev))
				{
					return false;
				}

				const std::string topdir = FindMountPoint(src, srcStat.st_dev);
				trashDir = ((topdir == "/") ? std::string{} : topdir) + "/.Trash-" + std::to_string(::getuid());

				if (not MakeDirectory700(trashDir))
				{
					return false;
				}

				infoPath = fs::path(src).lexically_relative(topdir).string();
			}

			const std::string filesDir = (trashDir + "/files");
			const std::string infoDir = (trashDir + "/info");

			if ((not MakeDirectory700(filesDir)) || (not MakeDirectory700(infoDir)))
			{
				return false;
			}

			// 日時 (ローカル時刻、YYYY-MM-DDThh:mm:ss)
			char dateBuffer[32] = {};
			{
				const std::time_t now = std::time(nullptr);
				std::tm local{};
				::localtime_r(&now, &local);
				std::strftime(dateBuffer, sizeof(dateBuffer), "%Y-%m-%dT%H:%M:%S", &local);
			}

			const std::string info = ("[Trash Info]\nPath=" + TrashURLEncode(infoPath) + "\nDeletionDate=" + dateBuffer + "\n");

			const std::string baseName = fs::path(src).filename().string();

			// 仕様どおり、先に .trashinfo を O_EXCL で作って名前を確保してから本体を移す
			for (int n = 1; n < 10000; ++n)
			{
				std::string name = baseName;

				if (2 <= n)
				{
					const fs::path p{ baseName };
					name = (p.stem().string() + "." + std::to_string(n) + p.extension().string());
				}

				const std::string infoFile = (infoDir + "/" + name + ".trashinfo");
				const std::string destFile = (filesDir + "/" + name);

				struct stat dummy;
				if (::lstat(destFile.c_str(), &dummy) == 0)
				{
					continue;
				}

				const int fd = ::open(infoFile.c_str(), (O_WRONLY | O_CREAT | O_EXCL), 0600);

				if (fd < 0)
				{
					if (errno == EEXIST)
					{
						continue;
					}

					return false;
				}

				const bool written = (::write(fd, info.data(), info.size()) == static_cast<ssize_t>(info.size()));
				::close(fd);

				if (written && (::rename(src.c_str(), destFile.c_str()) == 0))
				{
					return true;
				}

				::unlink(infoFile.c_str());
				return false;
			}

			return false;
		}

	# else

		// Web (Emscripten): ゴミ箱は無い (元に戻せる削除はできないので失敗を返す)
		[[nodiscard]]
		static bool TrashFile(const FilePathView)
		{
			return false;
		}

	# endif

		[[nodiscard]]
		FilePath AddSlash(FilePath path)
		{
			if (path.ends_with(U'/'))
			{
				return path;
			}

			return path + U'/';
		}

		namespace init
		{
			const static FilePath g_initialPath = NormalizePath(Unicode::Widen(fs::current_path().string()));

			static FilePath g_modulePath;

			void InitModulePath(const char* arg)
			{
				// ModulePath は全 OS で「実行ファイルのフルパス」('/' 区切り) にそろえる。
				// Resource() は ParentPath(ModulePath()) + "resources/" = 実行ファイルの隣を見る。
			# if SIV3D_PLATFORM(WINDOWS)
				(void)arg;
				std::wstring buf(MAX_PATH, L'\0');
				for (;;)
				{
					const DWORD len = ::GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
					if (len < buf.size())
					{
						buf.resize(len);
						break;
					}
					buf.resize(buf.size() * 2);
				}
				g_modulePath = Unicode::FromWstring(buf).replaced(U'\\', U'/');
			# elif SIV3D_PLATFORM(MACOS)
				// argv[0] はパス無しのコマンド名や相対パスのことがあるので、実行ファイルの絶対パスを使う
				char pathBuf[PATH_MAX];
				uint32_t size = sizeof(pathBuf);
				char resolved[PATH_MAX];

				if ((_NSGetExecutablePath(pathBuf, &size) == 0) && ::realpath(pathBuf, resolved))
				{
					g_modulePath = Unicode::Widen(resolved);
				}
				else
				{
					g_modulePath = Unicode::Widen(arg);
				}
			# else
				std::error_code ec;
				const fs::path exe = fs::read_symlink("/proc/self/exe", ec);
				g_modulePath = ((not ec) && (not exe.empty())) ? Unicode::Widen(exe.string()) : Unicode::Widen(arg);
			# endif
			}

		# if SIV3D_PLATFORM(WINDOWS)

			[[nodiscard]]
			static FilePath GetKnownFolder(const KNOWNFOLDERID& id)
			{
				PWSTR path = nullptr;
				if (SUCCEEDED(::SHGetKnownFolderPath(id, 0, nullptr, &path)))
				{
					FilePath result = Unicode::FromWstring(path);
					result.replace(U'\\', U'/');
					if (not result.ends_with(U'/'))
					{
						result.push_back(U'/');
					}
					::CoTaskMemFree(path);
					return result;
				}
				return {};
			}

			const static std::array<FilePath, 11> g_specialFolderPaths = []()
			{
				std::array<FilePath, 11> specialFolderPaths;

				specialFolderPaths[FromEnum(SpecialFolder::Desktop)]		= GetKnownFolder(FOLDERID_Desktop);
				specialFolderPaths[FromEnum(SpecialFolder::Documents)]		= GetKnownFolder(FOLDERID_Documents);
				specialFolderPaths[FromEnum(SpecialFolder::LocalAppData)]	= GetKnownFolder(FOLDERID_LocalAppData);
				specialFolderPaths[FromEnum(SpecialFolder::Music)]			= GetKnownFolder(FOLDERID_Music);
				specialFolderPaths[FromEnum(SpecialFolder::Pictures)]		= GetKnownFolder(FOLDERID_Pictures);
				specialFolderPaths[FromEnum(SpecialFolder::Videos)]			= GetKnownFolder(FOLDERID_Videos);
				specialFolderPaths[FromEnum(SpecialFolder::ProgramFiles)]	= GetKnownFolder(FOLDERID_ProgramFiles);
				specialFolderPaths[FromEnum(SpecialFolder::SystemFonts)]	= GetKnownFolder(FOLDERID_Fonts);
				specialFolderPaths[FromEnum(SpecialFolder::LocalFonts)]	= GetKnownFolder(FOLDERID_Fonts);
				specialFolderPaths[FromEnum(SpecialFolder::UserFonts)]		= GetKnownFolder(FOLDERID_Fonts);
				specialFolderPaths[FromEnum(SpecialFolder::UserProfile)]	= GetKnownFolder(FOLDERID_Profile);

				return specialFolderPaths;
			}();

		# else

			const static std::array<FilePath, 11> g_specialFolderPaths = []()
			{
				std::array<FilePath, 11> specialFolderPaths;

				specialFolderPaths[FromEnum(SpecialFolder::ProgramFiles)]	= U"/usr/";
				specialFolderPaths[FromEnum(SpecialFolder::LocalAppData)]	= U"/var/cache/";
				specialFolderPaths[FromEnum(SpecialFolder::SystemFonts)]	= U"/usr/share/fonts/";

				const FilePath homeDirectory = EnvironmentVariable::Get(U"HOME");

				if (not homeDirectory)
				{
					return specialFolderPaths;
				}

				if (const FilePath localCacheDirectory = EnvironmentVariable::Get(U"XDG_CACHE_HOME"))
				{
					specialFolderPaths[FromEnum(SpecialFolder::LocalAppData)] = (localCacheDirectory + U'/');
				}
				else
				{
					specialFolderPaths[FromEnum(SpecialFolder::LocalAppData)] = (homeDirectory + U"/.cache/");
				}

				if (const FilePath localFontDirectory = homeDirectory + U"/usr/local/share/fonts/";
					FileSystem::Exists(localFontDirectory))
				{
					specialFolderPaths[FromEnum(SpecialFolder::LocalFonts)] = localFontDirectory;
					specialFolderPaths[FromEnum(SpecialFolder::UserFonts)] = localFontDirectory;
				}

				specialFolderPaths[FromEnum(SpecialFolder::UserProfile)] = (homeDirectory + U'/');

			# if SIV3D_PLATFORM(MACOS)
				specialFolderPaths[FromEnum(SpecialFolder::Desktop)]		= (homeDirectory + U"/Desktop/");
				specialFolderPaths[FromEnum(SpecialFolder::Documents)]		= (homeDirectory + U"/Documents/");
				specialFolderPaths[FromEnum(SpecialFolder::LocalAppData)]	= (homeDirectory + U"/Library/Caches/");
				specialFolderPaths[FromEnum(SpecialFolder::Music)]			= (homeDirectory + U"/Music/");
				specialFolderPaths[FromEnum(SpecialFolder::Pictures)]		= (homeDirectory + U"/Pictures/");
				specialFolderPaths[FromEnum(SpecialFolder::Videos)]			= (homeDirectory + U"/Movies/");
				specialFolderPaths[FromEnum(SpecialFolder::ProgramFiles)]	= U"/Applications/";
				specialFolderPaths[FromEnum(SpecialFolder::SystemFonts)]	= U"/System/Library/Fonts/";
				specialFolderPaths[FromEnum(SpecialFolder::LocalFonts)]		= U"/Library/Fonts/";
				specialFolderPaths[FromEnum(SpecialFolder::UserFonts)]		= (homeDirectory + U"/Library/Fonts/");
				return specialFolderPaths;
			# endif

			# if SIV3D_PLATFORM(WEB)
				// Web: ホームフォルダ (/home/<exeName>) は IndexedDB に保存される (SDL3/Web/persist.pre.js.in)。
				// セーブデータの置き場所として、ホームフォルダの下に各フォルダを用意する
				specialFolderPaths[FromEnum(SpecialFolder::Desktop)]		= (homeDirectory + U"/Desktop/");
				specialFolderPaths[FromEnum(SpecialFolder::Documents)]		= (homeDirectory + U"/Documents/");
				specialFolderPaths[FromEnum(SpecialFolder::Music)]			= (homeDirectory + U"/Music/");
				specialFolderPaths[FromEnum(SpecialFolder::Pictures)]		= (homeDirectory + U"/Pictures/");
				specialFolderPaths[FromEnum(SpecialFolder::Videos)]			= (homeDirectory + U"/Videos/");
				return specialFolderPaths;
			# endif

				const FilePath iniFilePath = homeDirectory + U"/.config/user-dirs.dirs";

				if (not FileSystem::Exists(iniFilePath))
				{
					return specialFolderPaths;
				}

				const INI ini(iniFilePath);
				specialFolderPaths[FromEnum(SpecialFolder::Desktop)]	= AddSlash(ini[U"XDG_DESKTOP_DIR"].removed(U'\"').replaced(U"$HOME", homeDirectory));
				specialFolderPaths[FromEnum(SpecialFolder::Documents)]	= AddSlash(ini[U"XDG_DOCUMENTS_DIR"].removed(U'\"').replaced(U"$HOME", homeDirectory));
				specialFolderPaths[FromEnum(SpecialFolder::Music)]		= AddSlash(ini[U"XDG_MUSIC_DIR"].removed(U'\"').replaced(U"$HOME", homeDirectory));
				specialFolderPaths[FromEnum(SpecialFolder::Pictures)]	= AddSlash(ini[U"XDG_PICTURES_DIR"].removed(U'\"').replaced(U"$HOME", homeDirectory));
				specialFolderPaths[FromEnum(SpecialFolder::Videos)]		= AddSlash(ini[U"XDG_VIDEOS_DIR"].removed(U'\"').replaced(U"$HOME", homeDirectory));

				return specialFolderPaths;
			}();

		# endif

			const static Array<FilePath> g_resourceFilePaths = []()
			{
			# if SIV3D_PLATFORM(IOS)
				Array<FilePath> paths = FileSystem::DirectoryContents(U"siv3d_resources/", Recursive::Yes); // SivResource_SDL3.cpp を参照
			# else
				Array<FilePath> paths = FileSystem::DirectoryContents(U"resources/", Recursive::Yes);
			# endif

				paths.remove_if(FileSystem::IsDirectory);
				
				paths.sort();
				
				return paths;
			}();

			const Array<FilePath>& GetResourceFilePaths() noexcept
			{
				return g_resourceFilePaths;
			}
		}
	}

	namespace FileSystem
	{
		bool IsResourcePath(const FilePathView path) noexcept
		{
			// ParentPath() は IsResourcePath() を呼ぶので (再帰になる) ここでは使わない
			const FilePath& modulePath = FileSystem::ModulePath();
		# if SIV3D_PLATFORM(IOS)
			const FilePath resourceDirectory = (modulePath.substr(0, (modulePath.lastIndexOf(U'/') + 1)) + U"siv3d_resources/"); // SivResource_SDL3.cpp を参照
		# else
			const FilePath resourceDirectory = (modulePath.substr(0, (modulePath.lastIndexOf(U'/') + 1)) + U"resources/");
		# endif

			return FullPath(path).starts_with(resourceDirectory);
		}

		bool Exists(const FilePathView path)
		{
			if (not path) SIV3D_UNLIKELY
			{
				return false;
			}

			return detail::Exists(path);
		}

		bool IsDirectory(const FilePathView path)
		{
			if (not path) SIV3D_UNLIKELY
			{
				return false;
			}

			return detail::IsDirectory(path);
		}

		bool IsFile(const FilePathView path)
		{
			if (not path) SIV3D_UNLIKELY
			{
				return false;
			}

			return detail::IsRegular(path);
		}

		bool IsResource(const FilePathView path)
		{
			return IsResourcePath(path)
				&& detail::Exists(path);
		}

		FilePath FullPath(const FilePathView path)
		{
			if (not path) SIV3D_UNLIKELY
			{
				return FilePath{};
			}

			if (path.contains(U'/'))
			{
				return detail::NormalizePath(Unicode::Widen(fs::weakly_canonical(detail::ToPath(path)).string()));
			}
			else
			{
				return detail::NormalizePath(Unicode::Widen(fs::weakly_canonical(detail::ToPath(U"./" + path)).string()));
			}
		}

		Platform::NativeFilePath NativePath(const FilePathView path)
		{
			if (not path) SIV3D_UNLIKELY
			{
				return Platform::NativeFilePath{};
			}

		# if SIV3D_PLATFORM(WINDOWS)
			return fs::weakly_canonical(detail::ToPath(path)).wstring();
		# else
			return fs::weakly_canonical(detail::ToPath(path)).string();
		# endif
		}

		FilePath VolumePath(const FilePathView path)
		{
		# if SIV3D_PLATFORM(WINDOWS)
			const FilePath fullPath = FullPath(path);
			if (fullPath.size() >= 3 && fullPath[1] == U':' && fullPath[2] == U'/')
			{
				return fullPath.substr(0, 3);
			}
			return U"/";
		# else
			(void)path;
			return U"/";
		# endif
		}

		FilePath PathAppend(const FilePathView lhs, const FilePathView rhs)
		{
			return FilePath{ (detail::ToPath(lhs) / detail::ToPath(rhs)).u32string() }.replace(U'\\', U'/');
		}

		bool IsEmptyDirectory(const FilePathView path)
		{
			if (not path) SIV3D_UNLIKELY
			{
				return false;
			}

			if (not detail::IsDirectory(path))
			{
				return false;
			}

			return (fs::directory_iterator(detail::ToPath(path)) == fs::directory_iterator());
		}

		int64 Size(const FilePathView path)
		{
			if (not path) SIV3D_UNLIKELY
			{
				return 0;
			}
			
			const auto fsPath = detail::ToPath(path);

			if (fs::is_regular_file(fsPath))
			{
				return static_cast<int64>(fs::file_size(fsPath));
			}
			else if (fs::is_directory(fsPath))
			{
				int64 result = 0;
				
				for (const auto& v : fs::recursive_directory_iterator(fsPath))
				{
					if (v.is_regular_file())
					{
						result += static_cast<int64>(v.file_size());
					}
				}
				
				return result;
			}
			else
			{
				return 0;
			}
		}

		int64 FileSize(const FilePathView path)
		{
			if (not path) SIV3D_UNLIKELY
			{
				return 0;
			}
			
			const auto fsPath = detail::ToPath(path);

			if (!fs::is_regular_file(fsPath))
			{
				return 0;
			}
			
			return static_cast<int64>(fs::file_size(fsPath));
		}

		Optional<DateTime> CreationTime(const FilePathView path)
		{
		# if SIV3D_PLATFORM(WINDOWS)
			WIN32_FILE_ATTRIBUTE_DATA fad;
			if (!::GetFileAttributesExW(Unicode::ToWstring(path).c_str(), GetFileExInfoStandard, &fad))
			{
				return none;
			}
			return detail::ToDateTime(fad.ftCreationTime);
		# else
			struct stat s;
			if (!detail::GetStat(path, s))
			{
				return none;
			}
		# if SIV3D_PLATFORM(MACOS)
			return detail::ToDateTime(s.st_birthtimespec);
		# else
			return detail::ToDateTime(s.st_ctim);
		# endif
		# endif
		}

		Optional<DateTime> WriteTime(const FilePathView path)
		{
		# if SIV3D_PLATFORM(WINDOWS)
			WIN32_FILE_ATTRIBUTE_DATA fad;
			if (!::GetFileAttributesExW(Unicode::ToWstring(path).c_str(), GetFileExInfoStandard, &fad))
			{
				return none;
			}
			return detail::ToDateTime(fad.ftLastWriteTime);
		# else
			struct stat s;
			if (!detail::GetStat(path, s))
			{
				return none;
			}
		# if SIV3D_PLATFORM(MACOS)
			return detail::ToDateTime(s.st_mtimespec);
		# else
			return detail::ToDateTime(s.st_mtim);
		# endif
		# endif
		}
		
		Optional<DateTime> AccessTime(const FilePathView path)
		{
		# if SIV3D_PLATFORM(WINDOWS)
			WIN32_FILE_ATTRIBUTE_DATA fad;
			if (!::GetFileAttributesExW(Unicode::ToWstring(path).c_str(), GetFileExInfoStandard, &fad))
			{
				return none;
			}
			return detail::ToDateTime(fad.ftLastAccessTime);
		# else
			struct stat s;
			if (!detail::GetStat(path, s))
			{
				return none;
			}
		# if SIV3D_PLATFORM(MACOS)
			return detail::ToDateTime(s.st_atimespec);
		# else
			return detail::ToDateTime(s.st_atim);
		# endif
		# endif
		}

		Array<FilePath> DirectoryContents(const FilePathView path, const Recursive recursive)
		{
			Array<FilePath> paths;

			if (not path) SIV3D_UNLIKELY
			{
				return paths;
			}

			if (detail::GetStatus(path).type() != fs::file_type::directory)
			{
				return paths;
			}

			if (recursive)
			{
				for (const auto& v : fs::recursive_directory_iterator(path.narrow()))
				{
					paths.push_back(detail::NormalizePath(Unicode::Widen(fs::weakly_canonical(v.path()).string())));
				}
			}
			else
			{
				for (const auto& v : fs::directory_iterator(path.narrow()))
				{
					paths.push_back(detail::NormalizePath(Unicode::Widen(fs::weakly_canonical(v.path()).string())));
				}
			}

			return paths;
		}

		const FilePath& InitialDirectory() noexcept
		{
			return detail::init::g_initialPath;
		}

		const FilePath& ModulePath() noexcept
		{
			return detail::init::g_modulePath;
		}

		FilePath CurrentDirectory()
		{
			return detail::NormalizePath(Unicode::Widen(fs::current_path().string()));
		}

		bool ChangeCurrentDirectory(const FilePathView path)
		{
			if (not IsDirectory(path))
			{
				return false;
			}
			
		# if SIV3D_PLATFORM(WINDOWS)
			return (::_wchdir(Unicode::ToWstring(path).c_str()) == 0);
		# else
			return (chdir(path.narrow().c_str()) == 0);
		# endif
		}	

		const FilePath& GetFolderPath(const SpecialFolder folder)
		{
			assert(FromEnum(folder) < static_cast<int32>(std::size(detail::init::g_specialFolderPaths)));

			return detail::init::g_specialFolderPaths[FromEnum(folder)];
		}

		FilePath TemporaryDirectoryPath()
		{
		# if SIV3D_PLATFORM(WEB)
			// Web: ホームフォルダは IndexedDB に保存されるので、一時ファイルはその外 (メモリ上だけ) に置く
			return U"/tmp/";
		# else
			return (GetFolderPath(SpecialFolder::LocalAppData) + U"Temp/");
		# endif
		}

		FilePath RelativePath(const FilePathView _path, const FilePathView _start)
		{
			if (_path.isEmpty() || _start.isEmpty())
			{
				return FilePath();
			}

			const FilePath path = FullPath(_path);
			const FilePath start = FullPath(_start);

			if (not IsDirectory(start))
			{
				return path;
			}

			if (path == start)
			{
				return U"./";
			}

			const fs::path p(path.toUTF8()), base(start.toUTF8());
			FilePath result = Unicode::Widen(fs::proximate(p, base).string());

			if (IsDirectory(result) && (not result.ends_with(U'/')))
			{
				result.push_back(U'/');
			}

			return result;
		}

		bool CreateDirectories(const FilePathView path)
		{
			if (not path)
			{
				return true;
			}

			if (IsResourcePath(path))
			{
				return false;
			}

			try
			{
				fs::create_directories(detail::ToPath(path));
				return true;
			}
			catch (const fs::filesystem_error&)
			{
				return false;
			}
		}

		bool CreateParentDirectories(const FilePathView path)
		{
			if (not path)
			{
				return false;
			}

			if (IsResourcePath(path))
			{
				return false;
			}

			const FilePath parentDirectory = ParentPath(FullPath(path));

			if (not Exists(parentDirectory))
			{
				return CreateDirectories(parentDirectory);
			}

			return true;
		}
	
		bool Copy(const FilePathView from, const FilePathView to, const CopyOption copyOption)
		{
			if ((not from) || (not to))
			{
				return false;
			}

			if (IsResourcePath(from) || IsResourcePath(to))
			{
				return false;
			}

			CreateParentDirectories(to);

			const auto options = detail::ToCopyOptions(copyOption) | fs::copy_options::recursive;
			std::error_code error;
			fs::copy(detail::ToPath(from), detail::ToPath(to), options, error);

			return (error.value() == 0);
		}

		bool Remove(const FilePathView path, const AllowUndo allowUndo)
		{
			if (not path)
			{
				return false;
			}

			if (IsResourcePath(path))
			{
				return false;
			}

			if (not allowUndo)
			{
				try
				{
					fs::remove_all(detail::ToPath(path));
					return true;
				}
				catch (const fs::filesystem_error&)
				{
					return false;
				}
			}

			return detail::TrashFile(path);
		}

		bool RemoveContents(const FilePathView path, const AllowUndo allowUndo)
		{
			if (not IsDirectory(path))
			{
				return false;
			}

			if (not Remove(path, allowUndo))
			{
				return false;
			}

			return CreateDirectories(path);
		}

		bool Rename(const FilePathView from, const FilePathView to)
		{
			if ((not from) || (not to))
			{
				return false;
			}

			if (IsResourcePath(from) || IsResourcePath(to))
			{
				return false;
			}

			std::error_code error;
			std::filesystem::rename(detail::ToPath(from), detail::ToPath(to), error);

			return (error.value() == 0);
		}
	}
}
