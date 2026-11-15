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

# include <filesystem>
# include "DirectoryWatcherDetail.hpp"
# include <Siv3D/FileSystem.hpp>
# include <Siv3D/EngineLog.hpp>
# include <Siv3D/Unicode.hpp>

# if SIV3D_PLATFORM(WINDOWS)

////////////////////////////////////////////////////////////////
//
//  Windows implementation (ReadDirectoryChangesW)
//
////////////////////////////////////////////////////////////////

namespace s3d
{
	namespace detail
	{
		constexpr uint32 NotificationFilter = (FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_DIR_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE | FILE_NOTIFY_CHANGE_CREATION);

		[[nodiscard]]
		inline constexpr FileAction ToFileAction(DWORD action) noexcept
		{
			switch (action)
			{
			case FILE_ACTION_ADDED:
			case FILE_ACTION_RENAMED_NEW_NAME:
				return FileAction::Added;
			case FILE_ACTION_MODIFIED:
				return FileAction::Modified;
			case FILE_ACTION_REMOVED:
			case FILE_ACTION_RENAMED_OLD_NAME:
				return FileAction::Removed;
			default:
				return FileAction::Unknown;
			}
		}
	}

	DirectoryWatcher::DirectoryWatcherDetail::DirectoryWatcherDetail(const FilePathView directory)
	{
		if (directory.isEmpty()
			|| (not FileSystem::IsDirectory(directory)))
		{
			LOG_FAIL(U"❌ DirectoryWatcher: `{}` is not a directory"_fmt(directory));
			return;
		}

		m_targetDirectory = FileSystem::FullPath(directory);

		m_thread = std::jthread{ DirectoryWatcherDetail::Update, this };

		while (not m_initCalled)
		{
			::Sleep(1);
		}

		m_isActive = (m_directoryHandle != INVALID_HANDLE_VALUE);
	}

	DirectoryWatcher::DirectoryWatcherDetail::~DirectoryWatcherDetail()
	{
		if (m_thread.joinable())
		{
			m_thread.request_stop();
			m_thread.join();
		}

		if (m_directoryHandle == INVALID_HANDLE_VALUE)
		{
			return;
		}

		::CancelIoEx(m_directoryHandle, &m_overlapped);
		HANDLE tempDirectoryHandle = std::exchange(m_directoryHandle, INVALID_HANDLE_VALUE);
		::WaitForSingleObjectEx(tempDirectoryHandle, 1000, true);
		::CloseHandle(tempDirectoryHandle);

		LOG_INFO(U"ℹ️ DirectoryWatcher: Monitoring `{}` is deactivated"_fmt(m_targetDirectory));
	}

	bool DirectoryWatcher::DirectoryWatcherDetail::isActive() const
	{
		return m_isActive;
	}

	void DirectoryWatcher::DirectoryWatcherDetail::retrieveChanges(Array<FileChange>& fileChanges)
	{
		std::lock_guard lock{ m_changesMutex };
		fileChanges.assign(m_fileChanges.begin(), m_fileChanges.end());
		m_fileChanges.clear();
	}

	void DirectoryWatcher::DirectoryWatcherDetail::clearChanges()
	{
		std::lock_guard lock{ m_changesMutex };
		m_fileChanges.clear();
	}

	const FilePath& DirectoryWatcher::DirectoryWatcherDetail::directory() const noexcept
	{
		return m_targetDirectory;
	}

	bool DirectoryWatcher::DirectoryWatcherDetail::init()
	{
		m_overlapped.hEvent = this;
		m_buffer.resize(BufferSize);
		m_backBuffer.resize(BufferSize);

		m_directoryHandle = ::CreateFileW(
			m_targetDirectory.toWstr().c_str(),
			FILE_LIST_DIRECTORY,
			(FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE),
			nullptr,
			OPEN_EXISTING,
			(FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED),
			nullptr
		);

		if (m_directoryHandle == INVALID_HANDLE_VALUE)
		{
			LOG_FAIL(U"❌ DirectoryWatcher: Failed to create a directory handle `{}`"_fmt(m_targetDirectory));
			return false;
		}

		const bool result = !!::ReadDirectoryChangesW(
			m_directoryHandle,
			m_buffer.data(),
			static_cast<DWORD>(m_buffer.size()),
			WatchSubtree,
			detail::NotificationFilter,
			nullptr,
			&m_overlapped,
			&DirectoryWatcherDetail::OnChangeNotification);

		if (not result)
		{
			LOG_FAIL(U"❌ DirectoryWatcher: ReadDirectoryChangesW() failed. `{}`"_fmt(m_targetDirectory));
			::CloseHandle(m_directoryHandle);
			m_directoryHandle = INVALID_HANDLE_VALUE;
			return false;
		}

		LOG_INFO(U"ℹ️ DirectoryWatcher: Monitoring `{}` is activated"_fmt(m_targetDirectory));
		return true;
	}

	void DirectoryWatcher::DirectoryWatcherDetail::processChange(const DWORD errorCode, const DWORD numBytes)
	{
		if ((errorCode == ERROR_OPERATION_ABORTED) || (numBytes == 0))
		{
			return;
		}

		const bool isValid = (errorCode != ERROR_IO_INCOMPLETE);

		if (isValid)
		{
			m_backBuffer.swap(m_buffer);
		}

		const bool result = !!::ReadDirectoryChangesW(
			m_directoryHandle,
			m_buffer.data(),
			static_cast<DWORD>(m_buffer.size()),
			WatchSubtree,
			detail::NotificationFilter,
			nullptr,
			&m_overlapped,
			&DirectoryWatcherDetail::OnChangeNotification);

		if (not result)
		{
			LOG_FAIL(U"❌ DirectoryWatcher: ReadDirectoryChangesW() failed. `{}`"_fmt(m_targetDirectory));
			::CloseHandle(m_directoryHandle);
			m_directoryHandle = INVALID_HANDLE_VALUE;
			return;
		}

		if (not isValid)
		{
			return;
		}

		std::lock_guard lock{ m_changesMutex };

		for (const uint8* pInfo = m_backBuffer.data();;)
		{
			const FILE_NOTIFY_INFORMATION* notifyInfo = reinterpret_cast<const FILE_NOTIFY_INFORMATION*>(pInfo);
			const std::wstring_view view(notifyInfo->FileName, (notifyInfo->FileNameLength / sizeof(WCHAR)));
			const String localPath = Unicode::FromWstring(view).replace(L'\\', L'/');
			const FileAction action = detail::ToFileAction(notifyInfo->Action);

			m_fileChanges.emplace_back((m_targetDirectory + localPath), action);

			if (notifyInfo->NextEntryOffset == 0)
			{
				break;
			}

			pInfo += notifyInfo->NextEntryOffset;
		}
	}

	void DirectoryWatcher::DirectoryWatcherDetail::Update(std::stop_token stop_token, DirectoryWatcherDetail* watcher)
	{
		if (not watcher->init())
		{
			watcher->m_initCalled = true;
			return;
		}

		watcher->m_initCalled = true;

		while (not stop_token.stop_requested())
		{
			::SleepEx(15, true);
		}
	}

	void CALLBACK DirectoryWatcher::DirectoryWatcherDetail::OnChangeNotification(const DWORD dwErrorCode, const DWORD dwNumberOfBytesTransfered, const LPOVERLAPPED lpOverlapped)
	{
		DirectoryWatcherDetail* directoryWatcher = static_cast<DirectoryWatcherDetail*>(lpOverlapped->hEvent);
		directoryWatcher->processChange(dwErrorCode, dwNumberOfBytesTransfered);
	}
}

# elif SIV3D_PLATFORM(IOS)

////////////////////////////////////////////////////////////////
//
//  iOS: 未対応 (FSEvents が無い)。常に非アクティブで、変更は報告しない
//
////////////////////////////////////////////////////////////////

namespace s3d
{
	DirectoryWatcher::DirectoryWatcherDetail::DirectoryWatcherDetail(const FilePathView directory)
		: m_targetDirectory{ FileSystem::FullPath(directory) }
	{
		LOG_FAIL(U"❌ DirectoryWatcher: not supported on iOS (`{}`)"_fmt(m_targetDirectory));
	}

	DirectoryWatcher::DirectoryWatcherDetail::~DirectoryWatcherDetail() = default;

	bool DirectoryWatcher::DirectoryWatcherDetail::isActive() const
	{
		return false;
	}

	void DirectoryWatcher::DirectoryWatcherDetail::retrieveChanges(Array<FileChange>& fileChanges)
	{
		fileChanges.clear();
	}

	void DirectoryWatcher::DirectoryWatcherDetail::clearChanges()
	{
	}

	const FilePath& DirectoryWatcher::DirectoryWatcherDetail::directory() const noexcept
	{
		return m_targetDirectory;
	}
}

# elif SIV3D_PLATFORM(MACOS)

////////////////////////////////////////////////////////////////
//
//  macOS implementation (FSEvents)
//  (Siv3D-Platform/macOS の実装を移植)
//
////////////////////////////////////////////////////////////////

namespace s3d
{
	namespace detail
	{
		static void Convert(CFStringRef str, char32_t* dst)
		{
			const size_t Length = CFStringGetLength(str);
			CFRange Range = CFRangeMake(0, Length);
			CFIndex usedBufLen;
			CFStringGetBytes(str, Range, kCFStringEncodingUTF32LE, '?', false, (uint8*)dst, 2048, &usedBufLen);
			dst[usedBufLen / sizeof(char32_t)] = 0;
		}
	}

	DirectoryWatcher::DirectoryWatcherDetail::DirectoryWatcherDetail(const FilePathView directory)
	{
		if (directory.isEmpty()
			|| (not FileSystem::IsDirectory(directory)))
		{
			LOG_FAIL(U"❌ DirectoryWatcher: `{}` is not a directory"_fmt(directory));
			return;
		}
		
		m_targetDirectory = FileSystem::FullPath(directory);
		CFStringRef fullPath = CFStringCreateWithBytes(kCFAllocatorDefault,
													   reinterpret_cast<const uint8*>(m_targetDirectory.c_str()),
													   m_targetDirectory.size_bytes(), kCFStringEncodingUTF32LE, false);
		CFArrayRef pathToWatch = CFArrayCreate(NULL, (const void**)&fullPath, 1, NULL);
		
		FSEventStreamContext context = {
			.version			= 0,
			.info				= this,
			.retain				= NULL,
			.release			= NULL,
			.copyDescription	= NULL
		};
		
		m_eventStream = FSEventStreamCreate(NULL,
											&OnChange,
											&context,
											pathToWatch,
											kFSEventStreamEventIdSinceNow,
											LatencySec,
											(kFSEventStreamCreateFlagUseCFTypes | kFSEventStreamCreateFlagNoDefer | kFSEventStreamCreateFlagFileEvents)
											);
		
		CFRelease(pathToWatch);
		CFRelease(fullPath);
		
		if (not m_eventStream)
		{
			return;
		}
		
		FSEventStreamScheduleWithRunLoop(m_eventStream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
		FSEventStreamStart(m_eventStream);
		
		m_isActive = true;
		LOG_INFO(U"ℹ️ DirectoryWatcher: Monitoring `{}` is activated"_fmt(m_targetDirectory));
	}

	DirectoryWatcher::DirectoryWatcherDetail::~DirectoryWatcherDetail()
	{
		m_abort = true;
		
		if (not m_isActive)
		{
			return;
		}
		
		FSEventStreamStop(m_eventStream);
		FSEventStreamUnscheduleFromRunLoop(m_eventStream, CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
		FSEventStreamInvalidate(m_eventStream);
		FSEventStreamRelease(m_eventStream);
		
		m_isActive = false;
		LOG_INFO(U"ℹ️ DirectoryWatcher: Monitoring `{}` is deactivated"_fmt(m_targetDirectory));
	}

	bool DirectoryWatcher::DirectoryWatcherDetail::isActive() const
	{
		return m_isActive;
	}

	void DirectoryWatcher::DirectoryWatcherDetail::retrieveChanges(Array<FileChange>& fileChanges)
	{
		std::lock_guard lock{ m_changesMutex };

		fileChanges.assign(m_fileChanges.begin(), m_fileChanges.end());

		m_fileChanges.clear();
	}

	void DirectoryWatcher::DirectoryWatcherDetail::clearChanges()
	{
		std::lock_guard lock{ m_changesMutex };

		m_fileChanges.clear();
	}

	const FilePath& DirectoryWatcher::DirectoryWatcherDetail::directory() const noexcept
	{
		return m_targetDirectory;
	}

	void DirectoryWatcher::DirectoryWatcherDetail::OnChange(ConstFSEventStreamRef, void* pWatch, size_t eventCount, void* paths, const FSEventStreamEventFlags flags[], const FSEventStreamEventId[])
	{
		DirectoryWatcherDetail* watch = static_cast<DirectoryWatcherDetail*>(pWatch);
		
		watch->processChanges(eventCount, paths, flags);
	}

	void DirectoryWatcher::DirectoryWatcherDetail::processChanges(const size_t eventCount, void* paths, const FSEventStreamEventFlags flags[])
	{
		if (m_abort)
		{
			return;
		}
		
		char32_t buffer[2048];
		CFArrayRef eventPathArray = (CFArrayRef)paths;
		
		std::lock_guard lock{ m_changesMutex };
		
		for (size_t i = 0; i < eventCount; ++i)
		{
			const FSEventStreamEventFlags flag = flags[i];
			
			if(!(flag & kFSEventStreamEventFlagItemIsFile)
			   && !(flag & kFSEventStreamEventFlagItemIsDir))
			{
				continue;
			}
			
			const bool renamed	= (flag & kFSEventStreamEventFlagItemRenamed);
			const bool modified	= (flag & kFSEventStreamEventFlagItemModified);
			const bool added	= (flag & kFSEventStreamEventFlagItemCreated);
			const bool removed	= (flag & kFSEventStreamEventFlagItemRemoved);
			bool checkExistance	= false;
			
			FileAction action = FileAction::Unknown;
			
			if (modified)
			{
				action = FileAction::Modified;
				checkExistance = true;
			}
			else if (renamed)
			{
				action = FileAction::Added;
				checkExistance = true;
			}
			else if (added)
			{
				action = FileAction::Added;
				
				if (removed)
				{
					checkExistance = true;
				}
			}
			else if (removed)
			{
				action = FileAction::Removed;
			}
			else
			{
				continue;
			}
			
			detail::Convert((CFStringRef)CFArrayGetValueAtIndex(eventPathArray, i), buffer);
			
			FilePath path(buffer);
			
			if (checkExistance && !FileSystem::Exists(path))
			{
				action = FileAction::Removed;
			}
			
			m_fileChanges.push_back(FileChange{ std::move(path), action });
		}
	}
}

# else

////////////////////////////////////////////////////////////////
//
//  Linux/POSIX implementation (inotify)
//
////////////////////////////////////////////////////////////////

# include <sys/inotify.h>
# include <sys/select.h>
# include <unistd.h>

namespace s3d
{
	DirectoryWatcher::DirectoryWatcherDetail::DirectoryWatcherDetail(const FilePathView directory)
	{
		if (directory.isEmpty()
			|| (not FileSystem::IsDirectory(directory)))
		{
			LOG_FAIL(U"❌ DirectoryWatcher: `{}` is not a directory"_fmt(directory));
			return;
		}
		
		m_targetDirectory = FileSystem::FullPath(directory);

		m_buffer.resize(EventBufferSize);

		m_thread = std::thread(DirectoryWatcherDetail::watch, this);

		while (not m_initCalled)
		{
			::usleep(1000);
		}

		m_isActive = (true);
	}

	DirectoryWatcher::DirectoryWatcherDetail::~DirectoryWatcherDetail()
	{
		m_abort = true;

		if (m_thread.joinable())
		{
			m_thread.join();
		}

		dispose();

		LOG_INFO(U"ℹ️ DirectoryWatcher: Monitoring `{}` is deactivated"_fmt(m_targetDirectory));
	}

	bool DirectoryWatcher::DirectoryWatcherDetail::isActive() const
	{
		return m_isActive;
	}

	void DirectoryWatcher::DirectoryWatcherDetail::retrieveChanges(Array<FileChange>& fileChanges)
	{
		std::lock_guard lock{ m_changesMutex };

		fileChanges.assign(m_fileChanges.begin(), m_fileChanges.end());

		m_fileChanges.clear();
	}

	void DirectoryWatcher::DirectoryWatcherDetail::clearChanges()
	{
		std::lock_guard lock{ m_changesMutex };

		m_fileChanges.clear();
	}

	const FilePath& DirectoryWatcher::DirectoryWatcherDetail::directory() const noexcept
	{
		return m_targetDirectory;
	}

	void DirectoryWatcher::DirectoryWatcherDetail::watch(DirectoryWatcherDetail* watcher)
	{
		if (not watcher->init())
		{
			watcher->m_initCalled = true;
			return;
		}

		watcher->m_initCalled = true;

		while (not watcher->m_abort)
		{
			watcher->update();
		}
	}

	bool DirectoryWatcher::DirectoryWatcherDetail::init()
	{
		m_fd = inotify_init();
		if (m_fd < 0)
		{
			LOG_FAIL(U"❌ DirectoryWatcher: inotify_init() failed. `{}`"_fmt(m_targetDirectory));

			return false;
		}

		Array<FilePath> watched_directory_paths;
		watched_directory_paths.push_back(m_targetDirectory);

		namespace fs = std::filesystem;
		std::error_code error_code;
        const std::error_code no_error;
		fs::recursive_directory_iterator end;
		for (fs::recursive_directory_iterator itr(Unicode::Narrow(m_targetDirectory)); itr != end; itr.increment(error_code))
		{
			if (error_code != no_error)
			{
				dispose();
				LOG_FAIL(U"❌ DirectoryWatcher: filesystem::recursive_directory_iterator::increment() failed. `{}`"_fmt(m_targetDirectory));
				return false;
			}
			else
			{
				if (fs::is_directory(itr->path()))
				{
					watched_directory_paths.push_back(FileSystem::FullPath(Unicode::Widen(itr->path().c_str())));
				}
			}
		}

		for (auto itr = watched_directory_paths.cbegin(); itr != watched_directory_paths.cend(); ++itr)
		{
			int wd = inotify_add_watch(m_fd, Unicode::Narrow(*itr).c_str(), WatchMask);

			if (wd < 0)
			{
				dispose();
				LOG_FAIL(U"❌ DirectoryWatcher: inotify_add_watch() failed. `{}`"_fmt(*itr));
				return false;
			}

			m_watched_directories.insert(bimap_value_t(wd, *itr));
		}

		LOG_INFO(U"ℹ️ DirectoryWatcher: Monitoring `{}` is activated"_fmt(m_targetDirectory));

		return true;
	}

	void DirectoryWatcher::DirectoryWatcherDetail::update()
	{
		fd_set watch_set;
		FD_ZERO(&watch_set);
		FD_SET(m_fd, &watch_set);

		struct timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 10000; // 10ms

		int ret = select(m_fd + 1, &watch_set, NULL, NULL, &timeout);
		if (ret <= 0)
			return;

		if (!FD_ISSET(m_fd, &watch_set))
			return;

		int length = read(m_fd, m_buffer.data(), EventBufferSize);
		if (length < 0)
		{
			LOG_FAIL(U"❌ DirectoryWatcher: read() failed. `{}`"_fmt(m_targetDirectory));
			return;
		}

		struct inotify_event* event;
		for (int i = 0; i < length; i += (EventSize + event->len))
		{
			event = (struct inotify_event*)&m_buffer[i];

			FilePath current_path, event_path;
			FileAction action = FileAction::Unknown;

			if (event->wd == -1 || event->mask & IN_Q_OVERFLOW)
			{
				LOG_FAIL(U"❌ DirectoryWatcher: inotify event buffer overflowed. `{}`"_fmt(m_targetDirectory));
			}

			if (event->mask & IN_MODIFY)
			{
				event_path = m_watched_directories.left.at(event->wd) + Unicode::Widen(event->name);
				action = FileAction::Modified;
			}
			else if (event->mask & IN_CREATE || event->mask & IN_MOVED_TO)
			{
				current_path = m_watched_directories.left.at(event->wd);
				event_path = FileSystem::FullPath(current_path + Unicode::Widen(event->name));

				action = FileAction::Added;

				if (event->mask & IN_ISDIR)
				{
					int wd = inotify_add_watch(m_fd, Unicode::Narrow(event_path).c_str(), WatchMask);
					if (wd < 0)
					{
						LOG_FAIL(U"❌ DirectoryWatcher: inotify_add_watch() failed. `{}`"_fmt(event_path));
					}
					else
					{
						m_watched_directories.insert(bimap_value_t(wd, event_path));
					}
				}
				else
				{
					action = FileAction::Added;
				}
			}
			else if (event->mask & IN_DELETE || event->mask & IN_MOVED_FROM)
			{
				action = FileAction::Removed;

				if (event->mask & IN_ISDIR)
				{
					event_path = m_watched_directories.left.at(event->wd) + Unicode::Widen(event->name) + U'/';
				}
				else
				{
					event_path = m_watched_directories.left.at(event->wd) + Unicode::Widen(event->name);
				}
			}
			else if (event->mask & IN_DELETE_SELF)
			{
				inotify_rm_watch(m_fd, event->wd);
				continue;
			}
			else if (event->mask & IN_IGNORED)
			{
				event_path = m_watched_directories.left.at(event->wd);
				m_watched_directories.left.erase(event->wd);
				if (event_path == m_targetDirectory)
				{
					action = FileAction::Removed;
				}
				else
					continue;
			}

			{
				std::lock_guard lock{ m_changesMutex };
				m_fileChanges.push_back(FileChange{ std::move(event_path), action });
			}
		}
	}

	void DirectoryWatcher::DirectoryWatcherDetail::dispose()
	{
		if (not m_disposed)
		{
			m_disposed = true;

			for (auto itr = m_watched_directories.left.begin(); itr != m_watched_directories.left.end(); ++itr)
			{
				inotify_rm_watch(m_fd, itr->first);
			}
		}

        if (m_fd != -1)
        {
		    close(m_fd);
        }
	}
}

# endif
