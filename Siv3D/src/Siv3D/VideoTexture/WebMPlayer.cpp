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

// movie-player の公開ヘッダ (CMake でこのソースにだけインクルードパスを通している)
# include "IMoviePlayer.h"
# include "IAudioSink.h"

# include <atomic>
# include <deque>
# include <mutex>
# include <Siv3D/IAudioStream.hpp>
# include <Siv3D/EngineLog.hpp>
# include "WebMPlayer.hpp"
# include "../VideoReader/MovieProbe.hpp"

namespace s3d
{
	namespace
	{
		// movie-player の音声出力 (IAudioSink) を Siv3D の Audio のストリーム (IAudioStream) につなぐ。
		// - Enqueue された PCM はステレオの float に変換して保持し、実際に getAudio で読み出し終えた時点で
		//   param を consumed に返す (movie-player 側のバッファが返らないので、Audio を一時停止すると
		//   デコードも止まる = 際限なくたまらない)
		// - GetSamplesPlayed は getAudio で読み出したサンプル数。movie-player はこれを映像の時計に使う
		class AudioBridge final : public IAudioSink, public IAudioStream
		{
		public:

			//
			// IAudioSink (movie-player のデコードスレッドから呼ばれる)
			//

			bool Setup(const int channels, const int sampleRate, const int bitsPerSample, const Encoding encoding) override
			{
				if ((channels <= 0) || (sampleRate <= 0)
					|| (not (((encoding == PCM_S16) && (bitsPerSample == 16)) || ((encoding == PCM_F32) && (bitsPerSample == 32)))))
				{
					LOG_FAIL(U"WebMPlayer: unsupported audio format (channels: {}, sampleRate: {}, bits: {}, encoding: {})"_fmt(
						channels, sampleRate, bitsPerSample, static_cast<int32>(encoding)));
					return false;
				}

				m_channels = channels;
				m_sampleRate = sampleRate;
				m_encoding = encoding;
				return true;
			}

			void Enqueue(const void* data, const size_t bytes, const bool, void* param) override
			{
				const size_t bytesPerFrame = (m_channels * ((m_encoding == PCM_S16) ? sizeof(int16) : sizeof(float)));
				const size_t frames = ((data && bytesPerFrame) ? (bytes / bytesPerFrame) : 0);

				Entry entry{ .param = param };
				entry.samples.resize(frames * 2);

				for (size_t i = 0; i < frames; ++i)
				{
					const size_t src = (i * m_channels);
					const size_t srcRight = ((m_channels == 1) ? src : (src + 1));

					if (m_encoding == PCM_S16)
					{
						const int16* pcm = static_cast<const int16*>(data);
						entry.samples[i * 2] = (pcm[src] / 32768.0f);
						entry.samples[i * 2 + 1] = (pcm[srcRight] / 32768.0f);
					}
					else
					{
						const float* pcm = static_cast<const float*>(data);
						entry.samples[i * 2] = pcm[src];
						entry.samples[i * 2 + 1] = pcm[srcRight];
					}
				}

				std::lock_guard lock{ m_mutex };

				if (frames == 0)
				{
					m_consumed.push_back(param);
				}
				else
				{
					m_queue.push_back(std::move(entry));
				}
			}

			void Start() override
			{
				m_started = true;
			}

			void Stop() override
			{
				m_started = false;
			}

			int64_t GetSamplesPlayed() const override
			{
				return m_samplesPlayed;
			}

			bool TryPopConsumed(void** outParam) override
			{
				std::lock_guard lock{ m_mutex };

				if (m_consumed.empty())
				{
					return false;
				}

				*outParam = m_consumed.front();
				m_consumed.pop_front();
				return true;
			}

			void Flush() override
			{
				std::lock_guard lock{ m_mutex };

				for (const auto& entry : m_queue)
				{
					m_consumed.push_back(entry.param);
				}

				m_queue.clear();
			}

			void SetVolume(const float volume) override
			{
				m_volume = volume;
			}

			float Volume() const override
			{
				return m_volume;
			}

			//
			// IAudioStream (Siv3D のオーディオスレッドから呼ばれる)
			//

			void getAudio(float* left, float* right, const size_t samplesToWrite) override
			{
				size_t written = 0;

				if (m_started)
				{
					const float volume = m_volume;

					std::lock_guard lock{ m_mutex };

					while ((written < samplesToWrite) && (not m_queue.empty()))
					{
						Entry& entry = m_queue.front();
						const size_t available = ((entry.samples.size() / 2) - entry.offset);
						const size_t count = Min(available, (samplesToWrite - written));
						const float* src = (entry.samples.data() + entry.offset * 2);

						for (size_t i = 0; i < count; ++i)
						{
							left[written + i] = (src[i * 2] * volume);
							right[written + i] = (src[i * 2 + 1] * volume);
						}

						written += count;
						entry.offset += count;

						if (entry.offset == (entry.samples.size() / 2))
						{
							m_consumed.push_back(entry.param);
							m_queue.pop_front();
						}
					}

					m_samplesPlayed += written;
				}

				// 再生前・停止中・データ不足の分は無音
				for (size_t i = written; i < samplesToWrite; ++i)
				{
					left[i] = right[i] = 0.0f;
				}
			}

			bool hasEnded() override
			{
				return false;
			}

			void rewind() override {}

			[[nodiscard]]
			int32 sampleRate() const noexcept
			{
				return m_sampleRate;
			}

		private:

			struct Entry
			{
				// ステレオ (L, R) のインターリーブ
				Array<float> samples;

				size_t offset = 0;

				void* param = nullptr;
			};

			std::mutex m_mutex;

			std::deque<Entry> m_queue;

			std::deque<void*> m_consumed;

			std::atomic<int64> m_samplesPlayed{ 0 };

			std::atomic<bool> m_started{ false };

			std::atomic<float> m_volume{ 1.0f };

			int32 m_channels = 0;

			int32 m_sampleRate = 0;

			Encoding m_encoding = PCM_S16;
		};
	}

	class WebMPlayer::WebMPlayerDetail
	{
	public:

		~WebMPlayerDetail()
		{
			// コールバック (this を参照) と音声の橋渡しを使い終えてから破棄する
			if (m_player)
			{
				m_player->Stop();
				delete m_player;
				m_player = nullptr;
			}

			m_audio.stop();
		}

		[[nodiscard]]
		bool open(const FilePathView path, const bool loop)
		{
			if (not detail::IsEBML(path))
			{
				return false;
			}

			detail::MovieReadStream* stream = new detail::MovieReadStream{ path };

			if (not stream->isOpen())
			{
				stream->Release();
				return false;
			}

			// トラック構成を先に調べる。音声のコーデックは Opus (一般的) と Vorbis (Opus 非対応環境向け) の
			// どちらもありうるので、ここで判定してログに残す。音声トラックが無い (または非対応の) WebM は
			// VideoReader 経由 (音声なし) で扱う
			{
				WebmExtractor extractor;

				if (not extractor.Open(stream))
				{
					stream->Release();
					return false;
				}

				m_tracks = detail::ProbeWebM(extractor);
				stream->Seek(0, SEEK_SET);
			}

			if ((not m_tracks.hasVideo()) || (not m_tracks.hasAudio()))
			{
				LOG_INFO(U"ℹ️ VideoTexture: WebM has no supported audio track, playing without audio ({})"_fmt(detail::ToString(m_tracks)));
				stream->Release();
				return false;
			}

			m_bridge = std::make_shared<AudioBridge>();

			IMoviePlayer::InitParam param;
			param.Init();
			// バイト順 R, G, B, A (Siv3D の Image と同じ)
			param.videoColorFormat = IMoviePlayer::COLOR_RGBA;
			param.audioSink = m_bridge.get();

			m_player = IMoviePlayer::CreateMoviePlayer(stream, param);
			stream->Release();

			if (not m_player)
			{
				return false;
			}

			// 音声の無い動画は VideoReader 経由 (フレーム単位の読み出し) で扱う
			if ((not m_player->IsVideoAvailable()) || (not m_player->IsAudioAvailable()) || (m_bridge->sampleRate() <= 0))
			{
				delete m_player;
				m_player = nullptr;
				return false;
			}

			IMoviePlayer::VideoFormat videoFormat{};
			m_player->GetVideoFormat(&videoFormat);
			m_size = Size{ videoFormat.width, videoFormat.height };
			m_loop = loop;

			m_player->SetOnVideoDecoded([this](const int width, const int height, IMoviePlayer::DestUpdater updater)
			{
				std::lock_guard lock{ m_frameMutex };

				if ((m_backFrame.width() != width) || (m_backFrame.height() != height))
				{
					m_backFrame = Image{ Size{ width, height } };
				}

				updater(reinterpret_cast<char*>(m_backFrame.data()), (width * 4));
				m_hasNewFrame = true;
			});

			m_audio = Audio{ m_bridge, Arg::sampleRate = static_cast<uint32>(m_bridge->sampleRate()) };

			LOG_INFO(U"ℹ️ VideoTexture: WebM with audio (movie-player) {}, duration: {:.3f}s"_fmt(
				detail::ToString(m_tracks), lengthSec()));

			return true;
		}

		[[nodiscard]]
		Size size() const noexcept
		{
			return m_size;
		}

		[[nodiscard]]
		double lengthSec() const
		{
			return (m_player->Duration() / 1'000'000.0);
		}

		[[nodiscard]]
		double posSec() const
		{
			return (m_player->Position() / 1'000'000.0);
		}

		void play()
		{
			if (m_playing)
			{
				return;
			}

			m_player->Play(m_loop);
			m_audio.play();
			m_playing = true;
		}

		void seek(const double posSec)
		{
			m_player->Seek(static_cast<int64_t>(posSec * 1'000'000.0));
		}

		[[nodiscard]]
		bool fetchFrame(Image& image)
		{
			std::lock_guard lock{ m_frameMutex };

			if (not m_hasNewFrame)
			{
				return false;
			}

			image.swap(m_backFrame);
			m_hasNewFrame = false;
			return true;
		}

		[[nodiscard]]
		const Audio& audio() const noexcept
		{
			return m_audio;
		}

	private:

		IMoviePlayer* m_player = nullptr;

		detail::WebMTracks m_tracks;

		std::shared_ptr<AudioBridge> m_bridge;

		Audio m_audio;

		Size m_size{ 0, 0 };

		bool m_loop = false;

		bool m_playing = false;

		std::mutex m_frameMutex;

		Image m_backFrame;

		bool m_hasNewFrame = false;
	};

	std::unique_ptr<WebMPlayer> WebMPlayer::Create(const FilePathView path, const bool loop)
	{
		auto detail = std::make_unique<WebMPlayerDetail>();

		if (not detail->open(path, loop))
		{
			return nullptr;
		}

		return std::unique_ptr<WebMPlayer>{ new WebMPlayer{ std::move(detail) } };
	}

	WebMPlayer::WebMPlayer(std::unique_ptr<WebMPlayerDetail>&& detail)
		: pImpl{ std::move(detail) } {}

	WebMPlayer::~WebMPlayer() = default;

	Size WebMPlayer::size() const noexcept
	{
		return pImpl->size();
	}

	double WebMPlayer::lengthSec() const
	{
		return pImpl->lengthSec();
	}

	double WebMPlayer::posSec() const
	{
		return pImpl->posSec();
	}

	void WebMPlayer::play()
	{
		pImpl->play();
	}

	void WebMPlayer::seek(const double posSec)
	{
		pImpl->seek(posSec);
	}

	bool WebMPlayer::fetchFrame(Image& image)
	{
		return pImpl->fetchFrame(image);
	}

	const Audio& WebMPlayer::audio() const noexcept
	{
		return pImpl->audio();
	}
}
