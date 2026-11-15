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
# include "BinaryReaderDetail.hpp"

namespace s3d
{
	BinaryReader::BinaryReaderDetail::BinaryReaderDetail() = default;

	BinaryReader::BinaryReaderDetail::~BinaryReaderDetail()
	{
		close();
	}

	bool BinaryReader::BinaryReaderDetail::open(const FilePathView path)
	{
		LOG_TRACE(U"BinaryReader::BinaryReaderDetail::open(\"{0}\")"_fmt(path));

		close();

		// ファイルのオープン
		{
			// Android の SDL_IOFromFile は相対パスを APK の assets として開くので、絶対パスにして渡す
			// (Siv3D のファイルは内部ストレージに展開してある。他の OS でも結果は同じ)
			SDL_IOStream* io = SDL_IOFromFile(FileSystem::FullPath(path).narrow().c_str(), "rb");

			if (not io) SIV3D_UNLIKELY
			{
				LOG_FAIL(U"❌ BinaryReader: Failed to open the file `{0}`"_fmt(path));
				return false;
			}

			m_file =
			{
				.io = io
			};
		}

		m_info =
		{
			.isOpen		= true,
			.size		= FileSystem::FileSize(path),
			.fullPath	= FileSystem::FullPath(path)
		};

		LOG_INFO(U"📤 BinaryReader: File `{0}` opened (size: {1})"_fmt(
			m_info.fullPath, FormatDataSize(m_info.size)));

		return true;
	}

	void BinaryReader::BinaryReaderDetail::close()
	{
		if (not m_info.isOpen) SIV3D_UNLIKELY
		{
			return;
		}

		SDL_CloseIO(m_file.io);
		m_file.io = nullptr;
		LOG_INFO(U"📥 BinaryReader: File `{0}` closed"_fmt(
			m_info.fullPath));

		m_info = {};
	}

	bool BinaryReader::BinaryReaderDetail::isOpen() const noexcept
	{
		return m_info.isOpen;
	}

	int64 BinaryReader::BinaryReaderDetail::size() const noexcept
	{
		return m_info.size;
	}

	int64 BinaryReader::BinaryReaderDetail::setPos(const int64 clampedPos)
	{
		if (not m_info.isOpen) SIV3D_UNLIKELY
		{
			return 0;
		}

		assert(InRange<int64>(clampedPos, 0, size()));

		const Sint64 result = SDL_SeekIO(m_file.io, clampedPos, SDL_IO_SEEK_SET);
		return result;
	}

	int64 BinaryReader::BinaryReaderDetail::getPos()
	{
		return SDL_TellIO(m_file.io);
	}

	int64 BinaryReader::BinaryReaderDetail::read(const NonNull<void*> dst, const int64 size)
	{
		const int64 currentPos = SDL_TellIO(m_file.io);
		const int64 readBytes = Clamp<int64>(size, 0LL, (m_info.size - currentPos));

		if (readBytes)
		{
			const size_t bytesRead = SDL_ReadIO(m_file.io, dst.pointer, static_cast<size_t>(readBytes));

			if (bytesRead == 0 && SDL_GetIOStatus(m_file.io) == SDL_IO_STATUS_ERROR)
			{
				LOG_FAIL(U"❌ BinaryReader `{0}`: ReadFile() failed"_fmt(m_info.fullPath));
				return 0;
			}

			return static_cast<int64>(bytesRead);
		}

		return readBytes;
	}

	int64 BinaryReader::BinaryReaderDetail::read(const NonNull<void*> dst, const int64 pos, const int64 size)
	{
		if (pos != setPos(pos))
		{
			return 0;
		}

		const int64 currentPos = SDL_TellIO(m_file.io);
		const int64 readBytes = Clamp<int64>(size, 0LL, (m_info.size - currentPos));

		if (readBytes)
		{
			const size_t bytesRead = SDL_ReadIO(m_file.io, dst.pointer, static_cast<size_t>(readBytes));

			if (bytesRead == 0 && SDL_GetIOStatus(m_file.io) == SDL_IO_STATUS_ERROR)
			{
				LOG_FAIL(U"❌ BinaryReader `{0}`: ReadFile() failed"_fmt(m_info.fullPath));
				return 0;
			}

			return static_cast<int64>(bytesRead);
		}

		return readBytes;
	}

	int64 BinaryReader::BinaryReaderDetail::lookahead(const NonNull<void*> dst, const int64 size)
	{
		const auto previousPos = getPos();

		const int64 currentPos = SDL_TellIO(m_file.io);
		const int64 readBytes = Clamp<int64>(size, 0LL, (m_info.size - currentPos));

		if (readBytes)
		{
			const size_t bytesRead = SDL_ReadIO(m_file.io, dst.pointer, static_cast<size_t>(readBytes));

			if (bytesRead == 0 && SDL_GetIOStatus(m_file.io) == SDL_IO_STATUS_ERROR)
			{
				LOG_FAIL(U"❌ BinaryReader `{0}`: ReadFile() failed"_fmt(m_info.fullPath));
				setPos(previousPos);
				return 0;
			}

			setPos(previousPos);
			return static_cast<int64>(bytesRead);
		}

		setPos(previousPos);
		return readBytes;
	}

	int64 BinaryReader::BinaryReaderDetail::lookahead(const NonNull<void*> dst, const int64 pos, const int64 size)
	{
		const auto previousPos = getPos();

		if (pos != setPos(pos))
		{
			return 0;
		}

		const int64 currentPos = SDL_TellIO(m_file.io);
		const int64 readBytes = Clamp<int64>(size, 0LL, (m_info.size - currentPos));

		if (readBytes)
		{
			const size_t bytesRead = SDL_ReadIO(m_file.io, dst.pointer, static_cast<size_t>(readBytes));

			if (bytesRead == 0 && SDL_GetIOStatus(m_file.io) == SDL_IO_STATUS_ERROR)
			{
				LOG_FAIL(U"❌ BinaryReader `{0}`: ReadFile() failed"_fmt(m_info.fullPath));
				setPos(previousPos);
				return 0;
			}

			setPos(previousPos);
			return static_cast<int64>(bytesRead);
		}

		setPos(previousPos);
		return readBytes;
	}

	const FilePath& BinaryReader::BinaryReaderDetail::path() const noexcept
	{
		return m_info.fullPath;
	}
}
