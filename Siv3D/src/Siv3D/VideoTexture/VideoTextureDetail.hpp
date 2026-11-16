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
# include <Siv3D/VideoTexture.hpp>
# include <Siv3D/VideoReader.hpp>
# include <Siv3D/DynamicTexture.hpp>
# include <Siv3D/Audio.hpp>

namespace s3d
{
	class WebMPlayer;

	class VideoTexture::VideoTextureDetail
	{
	public:

		VideoTextureDetail();

		~VideoTextureDetail();

		bool load(FilePathView path, Loop loop, PlayAudio playAudio, TextureDesc desc);

		void advance(double deltaTimeSec = Scene::DeltaTime());

		void reset();

		[[nodiscard]]
		bool isLoop() const noexcept;

		[[nodiscard]]
		double posSec() const noexcept;

		void setPosSec(double posSec);

		[[nodiscard]]
		double lengthSec() const;

		[[nodiscard]]
		const Texture& getTexture() const noexcept;

		[[nodiscard]]
		const VideoReader& getVideoReader() const noexcept;

		[[nodiscard]]
		bool hasAudio() const noexcept;

		[[nodiscard]]
		const Audio& getAudio() const noexcept;

	private:

		VideoReader m_videoReader;

		// PlayAudio::Yes で音声付きの WebM を開いた場合に使う (映像も movie-player が再生する)。Web 版は常に使う
		std::unique_ptr<WebMPlayer> m_player;

		Audio m_emptyAudio;

		// m_player が音を鳴らすか (Web 版は PlayAudio::No でもプレーヤーで再生する)
		bool m_hasAudio = false;

		bool m_loop = false;

		bool m_isSRGB = false;

		double m_playTimeSec = 0.0;

		Image m_frameImage;

		size_t m_latestTextureIndex = 0;

		std::array<DynamicTexture, 2> m_frameTextures;

		bool m_hasDirty = false;

		void update(bool skipIfBusy);

		void uploadFrame(bool skipIfBusy);

		void updateFromPlayer();
	};
}
