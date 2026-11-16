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

# include "VideoTextureDetail.hpp"
# include <Siv3D/EngineLog.hpp>

// WebMPlayer を使うか: movie-player (SIV3D_WITH_MOVIEPLAYER) か、Web 版 (ブラウザの <video> で再生する実装)
# if defined(SIV3D_WITH_MOVIEPLAYER) || SIV3D_PLATFORM(WEB)
#	define SIV3D_VIDEO_USE_PLAYER 1
# endif

# if defined(SIV3D_VIDEO_USE_PLAYER)
#	include "WebMPlayer.hpp"
# else
namespace s3d
{
	// movie-player を使わないビルド (SIV3D_WITH_MOVIEPLAYER=OFF) では m_player は常に空。
	// std::unique_ptr<WebMPlayer> のデストラクタに完全型が要るので、空の定義を置く
	class WebMPlayer {};
}
# endif

namespace s3d
{
	VideoTexture::VideoTextureDetail::VideoTextureDetail() {}

	VideoTexture::VideoTextureDetail::~VideoTextureDetail() {}

	bool VideoTexture::VideoTextureDetail::load(const FilePathView path, const Loop loop, [[maybe_unused]] const PlayAudio playAudio, const TextureDesc desc)
	{
		m_loop = loop.getBool();
		m_isSRGB = detail::IsSRGB(desc);

	# if defined(SIV3D_VIDEO_USE_PLAYER)
	# if SIV3D_PLATFORM(WEB)
		// Web: VideoReader (OpenCV / movie-player) が無いので、常にブラウザの <video> で再生する
		// (PlayAudio::No なら音を出さない)。WebM のほか、ブラウザが再生できる形式 (MP4 など) も開ける
		{
			m_player = WebMPlayer::Create(path, m_loop, playAudio.getBool());
			m_hasAudio = playAudio.getBool();
	# else
		// 音声付きの WebM は movie-player のプレーヤーで映像と音声を同期して再生する
		if (playAudio)
		{
			m_player = WebMPlayer::Create(path, m_loop);
			m_hasAudio = true;
	# endif

			if (m_player)
			{
				// 最初のフレームがデコードされるまでは黒
				m_frameImage = Image{ m_player->size(), Palette::Black };
				m_frameTextures[m_latestTextureIndex] = DynamicTexture{ m_frameImage, (m_isSRGB ? TextureFormat::R8G8B8A8_Unorm_SRGB : TextureFormat::R8G8B8A8_Unorm) };
				return true;
			}
		}
	# endif

		if (not m_videoReader.open(path))
		{
			return false;
		}

		m_videoReader.readFrame(m_frameImage);
		m_frameTextures[m_latestTextureIndex] = DynamicTexture{ m_frameImage, (m_isSRGB ? TextureFormat::R8G8B8A8_Unorm_SRGB : TextureFormat::R8G8B8A8_Unorm) };

		return true;
	}

	void VideoTexture::VideoTextureDetail::advance(const double deltaTimeSec)
	{
	# if defined(SIV3D_VIDEO_USE_PLAYER)
		if (m_player)
		{
			// 再生位置は音声の時計に従うので deltaTimeSec は使わない
			m_player->play();
			updateFromPlayer();
			return;
		}
	# endif

		if (not m_videoReader)
		{
			return;
		}

		m_playTimeSec += deltaTimeSec;

		if (const double lengthSec = m_videoReader.getLengthSec();
			lengthSec <= m_playTimeSec)
		{
			if (m_loop)
			{
				m_playTimeSec = std::fmod(m_playTimeSec, lengthSec);
			}
			else
			{
				m_playTimeSec = lengthSec;
			}
		}

		update(true);
	}

	void VideoTexture::VideoTextureDetail::reset()
	{
	# if defined(SIV3D_VIDEO_USE_PLAYER)
		if (m_player)
		{
			m_player->seek(0.0);
			return;
		}
	# endif

		if (not m_videoReader)
		{
			return;
		}

		m_playTimeSec = 0.0;

		update(false);
	}

	bool VideoTexture::VideoTextureDetail::isLoop() const noexcept
	{
		return m_loop;
	}

	double VideoTexture::VideoTextureDetail::posSec() const noexcept
	{
	# if defined(SIV3D_VIDEO_USE_PLAYER)
		if (m_player)
		{
			return m_player->posSec();
		}
	# endif

		return m_playTimeSec;
	}

	void VideoTexture::VideoTextureDetail::setPosSec(const double posSec)
	{
	# if defined(SIV3D_VIDEO_USE_PLAYER)
		if (m_player)
		{
			m_player->seek(Clamp(posSec, 0.0, m_player->lengthSec()));
			return;
		}
	# endif

		m_playTimeSec = Clamp(posSec, 0.0, m_videoReader.getLengthSec());
	}

	double VideoTexture::VideoTextureDetail::lengthSec() const
	{
	# if defined(SIV3D_VIDEO_USE_PLAYER)
		if (m_player)
		{
			return m_player->lengthSec();
		}
	# endif

		return m_videoReader.getLengthSec();
	}

	const Texture& VideoTexture::VideoTextureDetail::getTexture() const noexcept
	{
		return m_frameTextures[m_latestTextureIndex];
	}

	const VideoReader& VideoTexture::VideoTextureDetail::getVideoReader() const noexcept
	{
		return m_videoReader;
	}

	bool VideoTexture::VideoTextureDetail::hasAudio() const noexcept
	{
		return (m_player && m_hasAudio);
	}

	const Audio& VideoTexture::VideoTextureDetail::getAudio() const noexcept
	{
	# if defined(SIV3D_VIDEO_USE_PLAYER)
		if (m_player)
		{
			return m_player->audio();
		}
	# endif

		return m_emptyAudio;
	}

	void VideoTexture::VideoTextureDetail::update(const bool skipIfBusy)
	{
		int32 targetFrameIndex = static_cast<int32>(m_playTimeSec / m_videoReader.getFrameDeltaSec());
		targetFrameIndex = Min(targetFrameIndex, static_cast<int32>(m_videoReader.getFrameCount()));

		const int32 currentFrameIndex = static_cast<int32>(m_videoReader.getCurrentFrameIndex());

		if (targetFrameIndex == currentFrameIndex)
		{
			if (m_hasDirty)
			{
				if (auto& texture = m_frameTextures[!m_latestTextureIndex];
					texture)
				{
					if (texture.fillIfNotBusy(m_frameImage))
					{
						m_latestTextureIndex = !m_latestTextureIndex;
						m_hasDirty = false;
					}
				}
				else
				{
					texture = DynamicTexture{ m_frameImage, (m_isSRGB ? TextureFormat::R8G8B8A8_Unorm_SRGB : TextureFormat::R8G8B8A8_Unorm) };
					m_latestTextureIndex = !m_latestTextureIndex;
					m_hasDirty = false;
				}
			}

			return;
		}
		else if (targetFrameIndex != (currentFrameIndex + 1))
		{
			m_videoReader.setCurrentFrameIndex(targetFrameIndex);
		}

		m_videoReader.readFrame(m_frameImage);

		uploadFrame(skipIfBusy);
	}

	void VideoTexture::VideoTextureDetail::uploadFrame(const bool skipIfBusy)
	{
		if (skipIfBusy)
		{
			if (auto& texture = m_frameTextures[!m_latestTextureIndex];
				texture)
			{
				if (texture.fillIfNotBusy(m_frameImage))
				{
					m_latestTextureIndex = !m_latestTextureIndex;
					m_hasDirty = false;
				}
				else
				{
					m_hasDirty = true;
				}
			}
			else
			{
				texture = DynamicTexture{ m_frameImage, (m_isSRGB ? TextureFormat::R8G8B8A8_Unorm_SRGB : TextureFormat::R8G8B8A8_Unorm) };
				m_latestTextureIndex = !m_latestTextureIndex;
				m_hasDirty = false;
			}
		}
		else
		{
			if (auto& texture = m_frameTextures[!m_latestTextureIndex];
				texture)
			{
				texture.fill(m_frameImage);
			}
			else
			{
				texture = DynamicTexture{ m_frameImage, (m_isSRGB ? TextureFormat::R8G8B8A8_Unorm_SRGB : TextureFormat::R8G8B8A8_Unorm) };
			}

			m_latestTextureIndex = !m_latestTextureIndex;
			m_hasDirty = false;
		}
	}

	void VideoTexture::VideoTextureDetail::updateFromPlayer()
	{
	# if defined(SIV3D_VIDEO_USE_PLAYER)
		if (m_player->fetchFrame(m_frameImage))
		{
			uploadFrame(true);
		}
		else if (m_hasDirty)
		{
			// 前回 GPU が使用中で書き込めなかったフレームを書き込み直す
			uploadFrame(true);
		}
	# endif
	}
}
