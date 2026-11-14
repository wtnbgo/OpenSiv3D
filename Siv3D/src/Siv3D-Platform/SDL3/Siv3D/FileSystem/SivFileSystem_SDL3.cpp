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
#	include <glib-2.0/glib.h>
#	include <glib-2.0/gio/gio.h>
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

	# else

		[[nodiscard]]
		static bool Linux_TrashFile(const char* path)
		{
			GFile* gf = g_file_new_for_path(path);
			GError* ge;

			gboolean ret = g_file_trash(gf, nullptr, &ge);

			if (ge)
			{
				g_error_free(ge);
			}
			
			g_object_unref(gf);

			if(!ret)
			{
				return false;
			}

			return true;
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
			# if SIV3D_PLATFORM(WINDOWS)
				(void)arg;
				wchar_t buf[MAX_PATH];
				::GetModuleFileNameW(nullptr, buf, MAX_PATH);
				g_modulePath = FilePath(Unicode::FromWstring(buf));
				// Remove filename, keep directory
				const size_t pos = g_modulePath.lastIndexOf(U'/');
				if (pos != String::npos)
				{
					g_modulePath = g_modulePath.substr(0, pos);
				}
			# elif SIV3D_PLATFORM(MACOS)
				// argv[0] は相対パスや .app 起動時の不定な値になりうるので、実行ファイルの絶対パスを使う
				char buf[PATH_MAX];
				uint32_t size = sizeof(buf);
				char resolved[PATH_MAX];

				if ((_NSGetExecutablePath(buf, &size) == 0) && ::realpath(buf, resolved))
				{
					g_modulePath = Unicode::Widen(resolved);
				}
				else
				{
					g_modulePath = Unicode::Widen(arg);
				}
			# else
				g_modulePath = Unicode::Widen(arg);
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
				Array<FilePath> paths = FileSystem::DirectoryContents(U"resources/", Recursive::Yes);

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
			const FilePath resourceDirectory = FileSystem::ModulePath() + U"/resources/";

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
			return (GetFolderPath(SpecialFolder::LocalAppData) + U"Temp/");
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

		# if SIV3D_PLATFORM(WINDOWS)
			return detail::TrashFile(path);
		# else
			return detail::Linux_TrashFile(path.narrow().c_str());
		# endif
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
