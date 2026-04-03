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

# include <Siv3D/EngineLog.hpp>
# include <Siv3D/FileSystem.hpp>
# include <Siv3D/FormatUtility.hpp>
# include "BinaryWriterDetail.hpp"

namespace s3d
{
	BinaryWriter::BinaryWriterDetail::BinaryWriterDetail() = default;

	BinaryWriter::BinaryWriterDetail::~BinaryWriterDetail()
	{
		close();
	}

	bool BinaryWriter::BinaryWriterDetail::open(const FilePathView path, const OpenMode openMode)
	{
		LOG_TRACE(U"BinaryWriter::BinaryWriterDetail::open(\"{0}\", {1})"_fmt(
			path, FromEnum(openMode)));

		close();

		if (not path)
		{
			LOG_FAIL(U"❌ BinaryWriter: path is empty");
			return false;
		}

		if (FileSystem::IsResourcePath(path))
		{
			LOG_FAIL(U"❌ BinaryWriter: path is a resource path");
			return false;
		}

		FilePath fullPath;
		const FilePath parentFilePath = FileSystem::ParentPath(path, 0, fullPath);

		if (parentFilePath && (not FileSystem::Exists(parentFilePath)) && (not FileSystem::CreateDirectories(parentFilePath)))
		{
			LOG_FAIL(U"❌ BinaryWriter: Failed to create parent directories \"{0}\""_fmt(parentFilePath));
			return false;
		}

		// ファイルのオープン
		{
			const bool append = (openMode == OpenMode::Append && FileSystem::Exists(fullPath));
			
			SDL_IOStream* io = SDL_IOFromFile(fullPath.narrow().c_str(), (append ? "rb+" : "wb"));

			if (not io)
			{
				LOG_FAIL(U"❌ BinaryWriter: Failed to open the file `{0}`"_fmt(path));
				return false;
			}

			m_file =
			{
				.io = io
			};

			if (openMode & OpenMode::Append)
			{
				SDL_SeekIO(m_file.io, 0, SDL_IO_SEEK_END);
			}

			m_info =
			{
				.fullPath = fullPath,
				.isOpen = true,
			};

			LOG_INFO(U"📤 BinaryWriter: File `{0}` opened"_fmt(
				m_info.fullPath));
		}

		return true;
	}

	void BinaryWriter::BinaryWriterDetail::close()
	{
		if (not m_info.isOpen) SIV3D_UNLIKELY
		{
			return;
		}

		flush();

		m_buffer = {};

		SDL_CloseIO(m_file.io);
		m_file = {};

		LOG_INFO(U"📥 BinaryWriter: File `{0}` closed"_fmt(
			m_info.fullPath));

		m_info = {};
	}

	bool BinaryWriter::BinaryWriterDetail::isOpen() const noexcept
	{
		return m_info.isOpen;
	}

	void BinaryWriter::BinaryWriterDetail::flush()
	{
		if (m_buffer.currentWritePos == 0)
		{
			return;
		}

		SDL_WriteIO(m_file.io, m_buffer.data.get(), m_buffer.currentWritePos);

		SDL_FlushIO(m_file.io);

		m_buffer.currentWritePos = 0;
	}

	void BinaryWriter::BinaryWriterDetail::clear()
	{
		if (not m_info.isOpen) SIV3D_UNLIKELY
		{
			return;
		}

		m_buffer.currentWritePos = 0;

		SDL_CloseIO(m_file.io);
		
		m_file.io = SDL_IOFromFile(m_info.fullPath.narrow().c_str(), "w");
		
		if (not m_file.io)
		{
			close();
		}
	}

	int64 BinaryWriter::BinaryWriterDetail::size()
	{
		if (not m_info.isOpen) SIV3D_UNLIKELY
		{
			return 0;
		}

		flush();

		return FileSystem::FileSize(m_info.fullPath);
	}

	int64 BinaryWriter::BinaryWriterDetail::setPos(const int64 clampedPos)
	{
		if (not m_info.isOpen) SIV3D_UNLIKELY
		{
			return 0;
		}
		
		flush();
		
		SDL_SeekIO(m_file.io, clampedPos, SDL_IO_SEEK_SET);
		
		return SDL_TellIO(m_file.io);
	}

	int64 BinaryWriter::BinaryWriterDetail::getPos()
	{
		if (not m_info.isOpen) SIV3D_UNLIKELY
		{
			return 0;
		}

		flush();

		return SDL_TellIO(m_file.io);
	}

	int64 BinaryWriter::BinaryWriterDetail::write(const NonNull<const void*> src, const size_t size)
	{
		if (not m_info.isOpen) SIV3D_UNLIKELY
		{
			return 0;
		}

		if (const size_t room = (BufferSize - m_buffer.currentWritePos);
			size <= room)
		{
			return fillBuffer(src, size);
		}
		
		flush();

		return static_cast<int64>(SDL_WriteIO(m_file.io, src.pointer, size));
	}

	const FilePath& BinaryWriter::BinaryWriterDetail::path() const noexcept
	{
		return m_info.fullPath;
	}

	int64 BinaryWriter::BinaryWriterDetail::fillBuffer(const NonNull<const void*> src, const size_t size)
	{
		if (!m_buffer.data)
		{
			m_buffer.data = std::make_unique<Byte[]>(BufferSize);
		}

		std::memcpy(m_buffer.data.get() + m_buffer.currentWritePos, src.pointer, size);

		m_buffer.currentWritePos += size;

		return size;
	}
}
