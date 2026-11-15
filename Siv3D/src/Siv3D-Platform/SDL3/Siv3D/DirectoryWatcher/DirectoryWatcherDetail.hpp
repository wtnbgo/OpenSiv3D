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

# pragma once
# include <Siv3D/DirectoryWatcher.hpp>
# if SIV3D_PLATFORM(WINDOWS)
#	include <Siv3D/Windows/Windows.hpp>
# elif SIV3D_PLATFORM(MACOS)
#	include <atomic>
#	include <mutex>
#	include <CoreServices/CoreServices.h>
# elif SIV3D_PLATFORM(LINUX)
#	include <thread>
#	include <atomic>
#	include <mutex>
#	include <boost/bimap.hpp>
#	include <sys/inotify.h>
# endif

namespace s3d
{
	class DirectoryWatcher::DirectoryWatcherDetail
	{
	public:

		explicit DirectoryWatcherDetail(FilePathView directory);

		~DirectoryWatcherDetail();

		bool isActive() const;

		void retrieveChanges(Array<FileChange>& fileChanges);

		void clearChanges();

		const FilePath& directory() const noexcept;

	private:

	# if SIV3D_PLATFORM(WINDOWS)

		static constexpr size_t MaxNotifyInformation = 10000;

		static constexpr size_t BufferSize = sizeof(FILE_NOTIFY_INFORMATION) * MaxNotifyInformation;

		static constexpr bool WatchSubtree = true;

		FilePath m_targetDirectory;

		std::jthread m_thread;

		std::atomic<bool> m_initCalled = false;

		bool m_isActive = false;

		OVERLAPPED m_overlapped = {};

		Array<uint8> m_buffer;

		Array<uint8> m_backBuffer;

		HANDLE m_directoryHandle = INVALID_HANDLE_VALUE;

		std::mutex m_changesMutex;

		Array<FileChange> m_fileChanges;

		bool init();

		void processChange(DWORD errorCode, DWORD numBytes);

		static void Update(std::stop_token stop_token, DirectoryWatcherDetail* watcher);

		static void CALLBACK OnChangeNotification(const DWORD dwErrorCode, const DWORD dwNumberOfBytesTransfered, const LPOVERLAPPED lpOverlapped);

	# elif SIV3D_PLATFORM(MACOS)

		static constexpr double LatencySec = 0.25;

		bool m_isActive = false;

		FilePath m_targetDirectory;

		FSEventStreamRef m_eventStream = nullptr;

		std::atomic<bool> m_abort = false;

		std::mutex m_changesMutex;

		Array<FileChange> m_fileChanges;

		static void OnChange(ConstFSEventStreamRef, void* pWatch, size_t eventCount, void* paths, const FSEventStreamEventFlags flags[], const FSEventStreamEventId[]);

		void processChanges(const size_t eventCount, void* paths, const FSEventStreamEventFlags flags[]);

	# elif SIV3D_PLATFORM(LINUX)

		using bimap_t = boost::bimap<int, FilePath>;
		using bimap_value_t = bimap_t::value_type;

		static constexpr size_t EventSize = sizeof(struct inotify_event);
		static constexpr size_t EventBufferSize = 2048 * (EventSize + 16);
		static constexpr uint32 WatchMask = IN_CREATE | IN_DELETE | IN_DELETE_SELF | IN_MODIFY | IN_MOVED_FROM | IN_MOVED_TO;

		FilePath m_targetDirectory;

		std::thread m_thread;

		std::atomic<bool> m_abort = false;
		std::atomic<bool> m_initCalled = false;

		bool m_isActive = false;
		bool m_disposed = false;

		int m_fd = -1;

		bimap_t m_watched_directories;

		Array<uint8> m_buffer;

		std::mutex m_changesMutex;

		Array<FileChange> m_fileChanges;

		static void watch(DirectoryWatcherDetail* watcher);

		bool init();

		void update();

		void dispose();

	# else

		// Web (Emscripten): ファイルの変更通知は無い (監視は常に無効)
		FilePath m_targetDirectory;

	# endif
	};
}
