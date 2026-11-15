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

// movie-player 連携の共通部品 (movie-player のインクルードパスが通ったソースからだけ使う)
# include "IMoviePlayer.h"

# include <atomic>
# include <cstdio>
# include <Siv3D/BinaryReader.hpp>

namespace s3d::detail
{
	// IMovieReadStream を Siv3D の BinaryReader で実装する (Unicode のパスやアーカイブ内のリソースも読めるように)
	class MovieReadStream final : public IMovieReadStream
	{
	public:

		explicit MovieReadStream(const FilePathView path)
			: m_reader{ path } {}

		[[nodiscard]]
		bool isOpen() const noexcept
		{
			return m_reader.isOpen();
		}

		int AddRef() override
		{
			return ++m_refCount;
		}

		int Release() override
		{
			const int count = --m_refCount;

			if (count == 0)
			{
				delete this;
			}

			return count;
		}

		size_t Read(void* buf, const size_t size) override
		{
			const int64 read = m_reader.read(buf, static_cast<int64>(size));
			return ((0 < read) ? static_cast<size_t>(read) : 0);
		}

		int64_t Tell() const override
		{
			return m_reader.getPos();
		}

		void Seek(const int64_t offset, const int origin) override
		{
			int64 pos = offset;

			if (origin == SEEK_CUR)
			{
				pos += m_reader.getPos();
			}
			else if (origin == SEEK_END)
			{
				pos += m_reader.size();
			}

			m_reader.setPos(pos);
		}

		size_t Size() const override
		{
			return static_cast<size_t>(m_reader.size());
		}

	private:

		mutable BinaryReader m_reader;

		std::atomic<int> m_refCount{ 1 };
	};

	// 先頭が EBML (Matroska / WebM) のマジックか
	[[nodiscard]]
	inline bool IsEBML(const FilePathView path)
	{
		BinaryReader reader{ path };

		uint8 magic[4] = {};

		if ((not reader) || (reader.read(magic, sizeof(magic)) != sizeof(magic)))
		{
			return false;
		}

		return ((magic[0] == 0x1A) && (magic[1] == 0x45) && (magic[2] == 0xDF) && (magic[3] == 0xA3));
	}
}
