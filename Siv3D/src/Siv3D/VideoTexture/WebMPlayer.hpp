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
# include <Siv3D/Audio.hpp>
# include <Siv3D/StringView.hpp>

namespace s3d
{
	/// @brief 音声付きの WebM を movie-player (IMoviePlayer) で再生するクラス (VideoTexture の PlayAudio::Yes 用)
	/// @remark 映像は音声の時計に同期してデコードされ、最新のフレームを fetchFrame で取り出す。
	/// 音声は IAudioSink → IAudioStream の橋渡しで Siv3D の Audio として鳴らす。
	class WebMPlayer
	{
	public:

		/// @brief 音声付きの WebM を開きます。
		/// @return WebM でない、音声トラックが無い、または開けない場合 nullptr
		[[nodiscard]]
		static std::unique_ptr<WebMPlayer> Create(FilePathView path, bool loop);

		~WebMPlayer();

		[[nodiscard]]
		Size size() const noexcept;

		[[nodiscard]]
		double lengthSec() const;

		[[nodiscard]]
		double posSec() const;

		/// @brief 再生を開始します (再生中なら何もしません)。
		void play();

		void seek(double posSec);

		/// @brief 前回から新しいフレームがデコードされていれば image に取り出します。
		/// @return 新しいフレームを取り出した場合 true
		[[nodiscard]]
		bool fetchFrame(Image& image);

		[[nodiscard]]
		const Audio& audio() const noexcept;

		class WebMPlayerDetail;

	private:

		explicit WebMPlayer(std::unique_ptr<WebMPlayerDetail>&& detail);

		std::unique_ptr<WebMPlayerDetail> pImpl;
	};
}
