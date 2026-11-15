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
# include <Siv3D/BinaryReader.hpp>
# include <Siv3D/Array.hpp>
# include "CAudioCodec.hpp"

//
// SDL3 版の AudioCodec。
// OS のコーデック (Media Foundation / FFmpeg 等) に頼らず、soloud に同梱されている
// dr_mp3 / dr_flac (実装は soloud の dr_impl.cpp でコンパイル済み) で全 OS 共通にデコードする。
// 対応: MP3, FLAC。AAC / WMA などは未対応 (空の Wave を返す)。
//

# include <ThirdParty/soloud/src/audiosource/wav/dr_mp3.h>
# include <ThirdParty/soloud/src/audiosource/wav/dr_flac.h>

namespace s3d
{
	namespace
	{
		[[nodiscard]]
		Array<Byte> ReadAll(IReader& reader)
		{
			const int64 size = reader.size();

			if (size <= 0)
			{
				return{};
			}

			Array<Byte> data(static_cast<size_t>(size));

			if (reader.read(data.data(), 0, size) != size)
			{
				return{};
			}

			return data;
		}

		// インターリーブされた float PCM を Wave (ステレオ) に変換する
		[[nodiscard]]
		Wave ToWave(const float* pcm, const uint64 frameCount, const uint32 channels, const uint32 sampleRate)
		{
			if ((pcm == nullptr) || (frameCount == 0) || (channels == 0) || (sampleRate == 0))
			{
				return{};
			}

			Wave wave(static_cast<size_t>(frameCount), Arg::sampleRate = sampleRate);

			if (channels == 1)
			{
				for (size_t i = 0; i < wave.size(); ++i)
				{
					wave[i].left = wave[i].right = pcm[i];
				}
			}
			else
			{
				// 3ch 以上は先頭の 2ch (L / R) を使う
				for (size_t i = 0; i < wave.size(); ++i)
				{
					wave[i].left = pcm[i * channels];
					wave[i].right = pcm[i * channels + 1];
				}
			}

			return wave;
		}

		[[nodiscard]]
		Wave DecodeMP3(const Array<Byte>& data)
		{
			drmp3_config config{};
			drmp3_uint64 frameCount = 0;
			float* pcm = drmp3_open_memory_and_read_pcm_frames_f32(data.data(), data.size(), &config, &frameCount, nullptr);

			if (pcm == nullptr)
			{
				LOG_FAIL(U"CAudioCodec: failed to decode MP3");
				return{};
			}

			Wave wave = ToWave(pcm, frameCount, config.channels, config.sampleRate);
			drmp3_free(pcm, nullptr);
			return wave;
		}

		[[nodiscard]]
		Wave DecodeFLAC(const Array<Byte>& data)
		{
			unsigned int channels = 0, sampleRate = 0;
			drflac_uint64 frameCount = 0;
			float* pcm = drflac_open_memory_and_read_pcm_frames_f32(data.data(), data.size(), &channels, &sampleRate, &frameCount, nullptr);

			if (pcm == nullptr)
			{
				LOG_FAIL(U"CAudioCodec: failed to decode FLAC");
				return{};
			}

			Wave wave = ToWave(pcm, frameCount, channels, sampleRate);
			drflac_free(pcm, nullptr);
			return wave;
		}
	}

	CAudioCodec::CAudioCodec() {}

	CAudioCodec::~CAudioCodec()
	{
		LOG_SCOPED_TRACE(U"CAudioCodec::~CAudioCodec()");
	}

	void CAudioCodec::init()
	{
		LOG_SCOPED_TRACE(U"CAudioCodec::init()");
	}

	Wave CAudioCodec::decode(IReader& reader, const AudioFormat audioFormat)
	{
		switch (audioFormat)
		{
		case AudioFormat::MP3:
			return DecodeMP3(ReadAll(reader));
		case AudioFormat::FLAC:
			return DecodeFLAC(ReadAll(reader));
		default:
			LOG_FAIL(U"CAudioCodec: unsupported audio format");
			return{};
		}
	}

	bool CAudioCodec::encode(const Wave&, IWriter&, AudioFormat)
	{
		// エンコードは未対応 (WAVE / OggVorbis / Opus は共通実装のエンコーダーがある)
		return false;
	}

	Wave CAudioCodec::decode(const FilePathView path, const AudioFormat audioFormat)
	{
		BinaryReader reader{ path };

		if (not reader)
		{
			return{};
		}

		return decode(reader, audioFormat);
	}
}
