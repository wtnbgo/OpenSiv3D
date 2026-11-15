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

//
// WebM (VP8 / VP9) の読み出し元。
// movie-player (https://github.com/wamsoft/movie-player) のデマルチプレクサ (WebmExtractor) と
// デコーダ (VpxDecoder) を、プレーヤー (時計駆動) を介さずに同期的に呼び出して 1 フレームずつ取り出す。
// VideoReader はフレーム単位の読み出し (readFrame / setCurrentFrameIndex) なので、この形の方が合う。
// 音声トラックは扱わない (VideoReader は映像のみ)。
//

// movie-player の内部ヘッダ (CMake でこのソースにだけインクルードパスを通している)
# include "WebmExtractor.h"
# include "VpxDecoder.h"
# include "FramePacket.h"

# include <atomic>
# include <cmath>
# include <cstring>
# include <Siv3D/BinaryReader.hpp>
# include <Siv3D/EngineLog.hpp>
# include <Siv3D/Unicode.hpp>
# include "VideoSource.hpp"
# include "MovieProbe.hpp"

namespace s3d
{
	namespace
	{
		class WebMVideoSource final : public IVideoSource
		{
		public:

			~WebMVideoSource() override
			{
				if (m_decoder)
				{
					m_decoder->Done();
				}
			}

			[[nodiscard]]
			bool open(const FilePathView path)
			{
				detail::MovieReadStream* stream = new detail::MovieReadStream{ path };

				if (not stream->isOpen())
				{
					stream->Release();
					return false;
				}

				// Open は stream を AddRef するので、こちらの参照はすぐ手放す
				const bool opened = m_extractor.Open(stream);
				stream->Release();

				if (not opened)
				{
					return false;
				}

				const detail::WebMTracks tracks = detail::ProbeWebM(m_extractor);

				if (not tracks.hasVideo())
				{
					LOG_FAIL(U"VideoReader: WebM has no supported video track ({})"_fmt(detail::ToString(tracks)));
					return false;
				}

				// 最初の VP8 / VP9 の映像トラックを使う (音声トラックは VideoReader では使わない)
				const size_t trackCount = m_extractor.GetTrackCount();

				for (size_t i = 0; i < trackCount; ++i)
				{
					TrackInfo track{};

					if ((not m_extractor.GetTrackInfo(static_cast<int32_t>(i), &track))
						|| (track.type != TRACK_TYPE_VIDEO)
						|| ((track.codecId != CODEC_V_VP8) && (track.codecId != CODEC_V_VP9)))
					{
						continue;
					}

					m_decoder = std::make_unique<VpxDecoder>(track.codecId);

					Decoder::Config config;
					config.Init(track.codecId);
					config.vpx.decCfg = {};
					config.vpx.decCfg.w = track.v.width;
					config.vpx.decCfg.h = track.v.height;
					config.vpx.decCfg.threads = Clamp(static_cast<int32>(std::thread::hardware_concurrency()) - 1, 1, 4);
					// バイト順 R, G, B, A (Siv3D の Image と同じ)
					config.vpx.rgbFormat = PIXEL_FORMAT_RGBA;
					config.vpx.alphaMode = track.v.alphaMode;

					if (not m_decoder->Configure(config))
					{
						m_decoder.reset();
						return false;
					}

					m_extractor.SelectTrack(TRACK_TYPE_VIDEO, static_cast<int32_t>(i));

					m_info.resolution = Size{ track.v.width, track.v.height };
					// TrackInfo の frameRate は DefaultDuration 要素から求めているが、要素が無い WebM では
					// 不定値になるので、妥当な範囲外なら先頭の映像パケットのタイムスタンプ間隔から求める
					m_info.fps = (InRange(track.v.frameRate, 1.0f, 480.0f) ? track.v.frameRate : estimateFPS());
					m_info.frameCount = static_cast<size_t>(std::llround(m_extractor.GetDurationUs() * m_info.fps / 1'000'000.0));
					m_hasAlpha = track.v.alphaMode;

					m_decoded.InitByType(TRACK_TYPE_VIDEO, 0);

					LOG_INFO(U"ℹ️ VideoReader: WebM (movie-player) {}, duration: {:.3f}s"_fmt(
						detail::ToString(tracks), (m_extractor.GetDurationUs() / 1'000'000.0)));

					return true;
				}

				LOG_FAIL(U"VideoReader: WebM file has no VP8 / VP9 video track");
				return false;
			}

			[[nodiscard]]
			const Info& info() const noexcept override
			{
				return m_info;
			}

			[[nodiscard]]
			bool readFrame(Image& image) override
			{
				while (not m_extractor.IsReachedEOS())
				{
					const TrackType type = m_extractor.NextFramePacketType();

					if (type != TRACK_TYPE_VIDEO)
					{
						if (m_extractor.IsReachedEOS())
						{
							break;
						}

						m_extractor.Advance();
						continue;
					}

					const bool hasPacket = m_extractor.ReadSampleData(&m_packet);
					m_extractor.Advance();

					if (not hasPacket)
					{
						break;
					}

					m_decoded.timeStampNs = static_cast<uint64_t>(-1);

					if (not m_decoder->DecodeFrame(&m_decoded, &m_packet))
					{
						continue;
					}

					// 表示しないフレーム (VP9 の非表示フレーム) は出力が無い
					if ((m_decoded.timeStampNs == static_cast<uint64_t>(-1)) || (m_decoded.data == nullptr))
					{
						continue;
					}

					// シーク直後は目的の時刻より前のフレームを読み捨てる
					if (static_cast<int64>(m_decoded.timeStampNs) < m_skipUntilNs)
					{
						continue;
					}

					m_skipUntilNs = 0;
					copyTo(image);
					return true;
				}

				return false;
			}

			void seekFrame(const int32 frameIndex) override
			{
				const double timeSec = (frameIndex / m_info.fps);
				const int64 timeUs = static_cast<int64>(timeSec * 1'000'000.0);

				// キーフレームへ戻ってから、目的のフレームの直前 (半フレーム手前) までを読み捨てる
				m_extractor.SeekTo(timeUs);
				m_skipUntilNs = static_cast<int64>((timeSec - (0.5 / m_info.fps)) * 1'000'000'000.0);
			}

		private:

			WebmExtractor m_extractor;

			std::unique_ptr<VpxDecoder> m_decoder;

			FramePacket m_packet;

			DecodedBuffer m_decoded;

			Info m_info;

			bool m_hasAlpha = false;

			int64 m_skipUntilNs = 0;

			// 先頭の映像パケットのタイムスタンプ間隔からフレームレートを求め、先頭に戻す
			[[nodiscard]]
			double estimateFPS()
			{
				Array<uint64> timeStamps;

				while ((timeStamps.size() < 16) && (not m_extractor.IsReachedEOS()))
				{
					if (m_extractor.NextFramePacketType() == TRACK_TYPE_VIDEO)
					{
						if (m_extractor.ReadSampleData(&m_packet))
						{
							timeStamps << m_packet.timeStampNs;
						}
					}

					m_extractor.Advance();
				}

				m_extractor.SeekTo(0);

				// タイムスタンプは TimecodeScale (通常 1ms) で丸められているので、1 区間ではなく全体の平均間隔を使う
				timeStamps.sort();

				if ((timeStamps.size() < 2) || (timeStamps.front() == timeStamps.back()))
				{
					return 30.0;
				}

				const double fps = ((timeStamps.size() - 1) * 1'000'000'000.0 / (timeStamps.back() - timeStamps.front()));

				LOG_INFO(U"ℹ️ VideoReader: WebM has no DefaultDuration, estimated fps: {:.3f}"_fmt(fps));

				return fps;
			}

			void copyTo(Image& image) const
			{
				const int32 width = m_decoded.v.width;
				const int32 height = m_decoded.v.height;

				if ((image.width() != width) || (image.height() != height))
				{
					image = Image{ Size{ width, height } };
				}

				std::memcpy(image.data(), m_decoded.v.planes[VDB_PLANE_PACKED], (static_cast<size_t>(width) * height * 4));
			}
		};
	}

	std::unique_ptr<IVideoSource> CreateWebMVideoSource(const FilePathView path)
	{
		if (not detail::IsEBML(path))
		{
			return nullptr;
		}

		auto source = std::make_unique<WebMVideoSource>();

		if (not source->open(path))
		{
			return nullptr;
		}

		return source;
	}
}
