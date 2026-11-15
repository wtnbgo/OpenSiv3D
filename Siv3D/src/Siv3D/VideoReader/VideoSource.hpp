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
# include <memory>
# include <Siv3D/Common.hpp>
# include <Siv3D/Image.hpp>
# include <Siv3D/StringView.hpp>

namespace s3d
{
	// VideoReader が使う動画の読み出し元 (OpenCV の VideoCapture / movie-player の WebM)
	// readFrame はバックグラウンドスレッドから呼ばれる (VideoReaderDetail の m_mutex の内側)
	class IVideoSource
	{
	public:

		struct Info
		{
			Size resolution = Size{ 0, 0 };

			double fps = 30.0;

			size_t frameCount = 0;
		};

		virtual ~IVideoSource() = default;

		[[nodiscard]]
		virtual const Info& info() const noexcept = 0;

		/// @brief 次のフレームを読み込みます。
		/// @param image 読み込んだフレームの格納先 (RGBA)
		/// @return 読み込めた場合 true、終端に達した場合 false
		[[nodiscard]]
		virtual bool readFrame(Image& image) = 0;

		/// @brief 次に readFrame で読み込むフレームを指定します。
		virtual void seekFrame(int32 frameIndex) = 0;
	};

	[[nodiscard]]
	std::unique_ptr<IVideoSource> CreateOpenCVVideoSource(FilePathView path);

# if defined(SIV3D_WITH_MOVIEPLAYER)

	/// @brief WebM (VP8 / VP9) を movie-player で読み込む読み出し元を作成します。
	/// @return WebM でない、または開けない場合 nullptr
	[[nodiscard]]
	std::unique_ptr<IVideoSource> CreateWebMVideoSource(FilePathView path);

# endif
}
