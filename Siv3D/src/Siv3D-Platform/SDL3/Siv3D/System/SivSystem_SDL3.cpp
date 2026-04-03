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
#	define SECURITY_WIN32
#	include <Security.h>
#	include <lmcons.h>
# else
#	include <fcntl.h>
#	include <signal.h>
#	include <sys/wait.h>
#	include <unistd.h>
#	include <pwd.h>
# endif

# include <Siv3D/System.hpp>
# include <Siv3D/FileSystem.hpp>
# include <Siv3D/Unicode.hpp>

namespace s3d
{
# if SIV3D_PLATFORM(WINDOWS)

	namespace
	{
		[[nodiscard]]
		static bool IsRunningInVisualStudio_impl()
		{
			wchar_t* pValue;
			size_t len;
			errno_t err = ::_wdupenv_s(&pValue, &len, L"VisualStudioVersion");

			if (err || (not pValue))
			{
				return false;
			}

			std::free(pValue);

			return true;
		}
	}

	namespace System
	{
		bool LaunchBrowser(const FilePathView _url)
		{
			String url{ _url };
			const bool isWebPage = url.starts_with(U"http://")
				|| url.starts_with(U"https://");

			if (not isWebPage)
			{
				const String extension = FileSystem::Extension(_url);
				const bool isHTML = (extension == U"html") || (extension == U"htm");

				if (not isHTML)
				{
					return false;
				}

				url = (U"file://" + FileSystem::FullPath(_url));
			}

			return reinterpret_cast<size_t>(::ShellExecuteW(nullptr, L"open", url.toWstr().c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32;
		}

		bool ShowInFileManager(const FilePathView path)
		{
			const FilePath fullPath = FileSystem::FullPath(path);

			if (FileSystem::IsFile(fullPath))
			{
				const String param = U"/select," + fullPath.replaced(U'/', U'\\');

				return (reinterpret_cast<int64>(::ShellExecuteW(nullptr, L"open", L"explorer.exe", param.toWstr().c_str(), nullptr, SW_SHOWNORMAL)) > 32);
			}
			else if (FileSystem::IsDirectory(fullPath))
			{
				return (reinterpret_cast<int64>(::ShellExecuteW(nullptr, L"explore", fullPath.toWstr().c_str(), nullptr, nullptr, SW_SHOWNORMAL)) > 32);
			}

			return false;
		}

		String ComputerName()
		{
			wchar_t buffer[MAX_COMPUTERNAME_LENGTH + 1];
			DWORD size = static_cast<DWORD>(std::ssize(buffer));

			if (::GetComputerNameExW(ComputerNamePhysicalDnsHostname, buffer, &size))
			{
				return Unicode::FromWstring(buffer);
			}
			else
			{
				return{};
			}
		}

		String UserName()
		{
			wchar_t buffer[UNLEN + 1];
			DWORD size = static_cast<DWORD>(std::ssize(buffer));

			if (GetUserNameW(buffer, &size))
			{
				return Unicode::FromWstring(buffer);
			}
			else
			{
				return{};
			}
		}

		String FullUserName()
		{
			wchar_t buffer[UNLEN + 1];
			ULONG size = static_cast<ULONG>(std::ssize(buffer));

			if (::GetUserNameExW(NameDisplay, buffer, &size))
			{
				return Unicode::FromWstring(buffer);
			}
			else
			{
				return{};
			}
		}

		String DefaultLocale()
		{
			wchar_t languageBuffer[LOCALE_NAME_MAX_LENGTH];

			if (const size_t length = ::GetUserDefaultLocaleName(languageBuffer, LOCALE_NAME_MAX_LENGTH))
			{
				return Unicode::FromWstring(std::wstring_view{ languageBuffer, (length - 1) });
			}
			else
			{
				return U"en-US";
			}
		}

		String DefaultLanguage()
		{
			ULONG numLanguages = 0;
			ULONG languageBufferSize = 0;

			if (::GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &numLanguages, nullptr, &languageBufferSize))
			{
				std::wstring languages(languageBufferSize, '\0');

				if (::GetUserPreferredUILanguages(MUI_LANGUAGE_NAME, &numLanguages, languages.data(), &languageBufferSize))
				{
					return Unicode::FromWstring(languages.data());
				}
			}

			return DefaultLocale();
		}

		bool LaunchFile(const FilePathView fileName)
		{
			const Platform::NativeFilePath fullpath = FileSystem::NativePath(fileName);

			const HINSTANCE result = ::ShellExecuteW(nullptr, nullptr, fullpath.c_str(), nullptr, nullptr, SW_SHOW);

			return (32 < static_cast<int32>(reinterpret_cast<size_t>(result)));
		}

		bool LaunchFileWithTextEditor(const FilePathView fileName)
		{
			const Platform::NativeFilePath fullpath = FileSystem::NativePath(fileName);

			SHELLEXECUTEINFOW sei{};
			sei.cbSize = sizeof(sei);
			sei.lpVerb = L"open";
			sei.lpFile = L"notepad.exe";
			sei.lpParameters = fullpath.c_str();
			sei.nShow = SW_SHOW;

			return (::ShellExecuteExW(&sei) != 0);
		}

		bool IsRunningInVisualStudio()
		{
			static const bool result = IsRunningInVisualStudio_impl();

			return result;
		}

		bool IsRunningInXcode()
		{
			return false;
		}
	}

# else // Linux / macOS

	namespace detail
	{
		[[nodiscard]]
		static std::string Escape(const std::string& s)
		{
			std::string result;
			result.push_back('\'');

			for (char ch : s)
			{
				if (ch == '\'')
				{
					result.append("'\\''");
				}
				else
				{
					result.push_back(ch);
				}
			}

			result.push_back('\'');
			return result;
		}

		[[nodiscard]]
		static bool Run(const char* program, char* argv[])
		{
			sigset_t set, oldset;
			sigemptyset(&set);
			sigaddset(&set, SIGCHLD);
			sigprocmask(SIG_BLOCK, &set, &oldset);

			pid_t pid;
			if (pid = fork(); pid == 0)
			{
				if (not fork())
				{
					close(0);
					open("/dev/null", O_RDONLY);
					close(1);
					open("/dev/null", O_WRONLY);
					close(2);
					open("/dev/null", O_WRONLY);

					setsid();

					execv(program, argv);
					_exit(0);
				}
				else
				{
					_exit(0);
				}
			}
			else if (pid < 0)
			{
				sigprocmask(SIG_SETMASK, &oldset, 0);
				return false;
			}

			int status = 0;
			while (waitpid(pid, &status, 0) < 0)
			{
				if (errno != EINTR)
				{
					sigprocmask(SIG_SETMASK, &oldset, 0);
					return false;
				}
			}

			sigprocmask(SIG_SETMASK, &oldset, 0);
			return true;
		}
	}

	namespace System
	{
		bool LaunchBrowser(const FilePathView _url)
		{
			String url{ _url };
			const bool isWebPage = url.starts_with(U"http://")
				|| url.starts_with(U"https://");

			if (not isWebPage)
			{
				if (not FileSystem::IsFile(_url))
				{
					return false;
				}

				const String extension = FileSystem::Extension(_url);
				const bool isHTML = (extension == U"html") || (extension == U"htm");

				if (not isHTML)
				{
					return false;
				}

				url = FileSystem::FullPath(_url);
			}

			std::string urlc = url.toUTF8();
			char* argv[] = { (char*)"xdg-open", urlc.data(), nullptr };
			return detail::Run("/usr/bin/xdg-open", argv);
		}

		bool ShowInFileManager(const FilePathView path)
		{
			if (not FileSystem::Exists(path))
			{
				return false;
			}

			std::string pathc = path.toUTF8();
			char* argv[] = { (char*)"nautilus", pathc.data(), nullptr };
			return detail::Run("/usr/bin/nautilus", argv);
		}

		String ComputerName()
		{
			char name[256];

			if (gethostname(name, sizeof(name)) == 0)
			{
				return Unicode::Widen(name);
			}
			else
			{
				return{};
			}
		}

		String UserName()
		{
			if (const char* username = std::getenv("USER"))
			{
				return Unicode::Widen(username);
			}
			else if (const char* username = std::getenv("USERNAME"))
			{
				return Unicode::Widen(username);
			}
			else if (const char* username = std::getenv("LOGNAME"))
			{
				return Unicode::Widen(username);
			}
			else if (const passwd* pw = getpwuid(getuid()))
			{
				return Unicode::Widen(pw->pw_name);
			}
			else
			{
				return{};
			}
		}

		String FullUserName()
		{
			if (const passwd* pw = getpwuid(getuid()))
			{
				std::string gecos = pw->pw_gecos;

				if (const size_t pos = gecos.find_first_of(',');
					pos != std::string::npos)
				{
					gecos.resize(pos);
				}

				return Unicode::Widen(gecos);
			}
			else
			{
				return{};
			}
		}

		String DefaultLocale()
		{
			if (const char* locale = setlocale(LC_ALL, ""))
			{
				return Unicode::Widen(locale).replace(U'_', U'-');
			}
			else
			{
				return U"en-US";
			}
		}

		String DefaultLanguage()
		{
			if (const char* language = std::getenv("LANG"))
			{
				return Unicode::Widen(language).replace(U'_', U'-');
			}
			else
			{
				return U"en-US";
			}
		}

		bool LaunchFile(const FilePathView fileName)
		{
			const std::string command = ("xdg-open " + detail::Escape(fileName.narrow()));

			return (std::system(command.c_str()) == 0);
		}

		bool LaunchFileWithTextEditor(const FilePathView fileName)
		{
			const char* editor = std::getenv("VISUAL");

			if (editor == nullptr)
			{
				editor = std::getenv("EDITOR");

				if (editor == nullptr)
				{
					// gedit is the default text editor in Ubuntu
					if (std::system("which gedit >/dev/null 2>&1") == 0)
					{
						editor = "gedit";
					}
					else
					{
						editor = "vi";
					}
				}
			}

			const std::string command = (std::string(editor) + " " + detail::Escape(fileName.narrow()));

			return (std::system(command.c_str()) == 0);
		}

		bool IsRunningInVisualStudio()
		{
			return false;
		}

		bool IsRunningInXcode()
		{
			return false;
		}
	}

# endif
}
