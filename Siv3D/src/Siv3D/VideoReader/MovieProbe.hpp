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

// movie-player 連携の共通部品 (movie-player の内部ヘッダのインクルードパスが通ったソースからだけ使う)
# include "WebmExtractor.h"

# include <Siv3D/String.hpp>
# include <Siv3D/FormatLiteral.hpp>
# include "MovieReadStream.hpp"

namespace s3d::detail
{
	/// @brief WebM のトラック構成 (最初の映像トラックと最初の音声トラック)
	struct WebMTracks
	{
		CodecId videoCodec = CODEC_UNKNOWN;

		int32 width = 0;

		int32 height = 0;

		bool alpha = false;

		CodecId audioCodec = CODEC_UNKNOWN;

		int32 channels = 0;

		int32 sampleRate = 0;

		[[nodiscard]]
		bool hasVideo() const noexcept
		{
			return ((videoCodec == CODEC_V_VP8) || (videoCodec == CODEC_V_VP9));
		}

		[[nodiscard]]
		bool hasAudio() const noexcept
		{
			return ((audioCodec == CODEC_A_VORBIS) || (audioCodec == CODEC_A_OPUS));
		}
	};

	[[nodiscard]]
	inline StringView CodecName(const CodecId codec) noexcept
	{
		switch (codec)
		{
		case CODEC_V_VP8:
			return U"VP8";
		case CODEC_V_VP9:
			return U"VP9";
		case CODEC_V_AV1:
			return U"AV1";
		case CODEC_A_VORBIS:
			return U"Vorbis";
		case CODEC_A_OPUS:
			return U"Opus";
		default:
			return U"unknown";
		}
	}

	/// @brief WebM のトラック構成を調べます。
	/// @remark 音声のコーデックはファイルによって Opus (現在の一般的なエンコード) と
	/// Vorbis (Opus に対応しない環境向けのエンコード) のどちらもあるので、ここで判定する。
	[[nodiscard]]
	inline WebMTracks ProbeWebM(WebmExtractor& extractor)
	{
		WebMTracks tracks;

		const size_t trackCount = extractor.GetTrackCount();

		for (size_t i = 0; i < trackCount; ++i)
		{
			TrackInfo info{};

			if (not extractor.GetTrackInfo(static_cast<int32_t>(i), &info))
			{
				continue;
			}

			if ((info.type == TRACK_TYPE_VIDEO) && (tracks.videoCodec == CODEC_UNKNOWN))
			{
				tracks.videoCodec = info.codecId;
				tracks.width = info.v.width;
				tracks.height = info.v.height;
				tracks.alpha = info.v.alphaMode;
			}
			else if ((info.type == TRACK_TYPE_AUDIO) && (tracks.audioCodec == CODEC_UNKNOWN))
			{
				tracks.audioCodec = info.codecId;
				tracks.channels = info.a.channels;
				tracks.sampleRate = static_cast<int32>(info.a.sampleRate);
			}
		}

		return tracks;
	}

	[[nodiscard]]
	inline String ToString(const WebMTracks& tracks)
	{
		String s = U"video: {} {}x{}{}"_fmt(CodecName(tracks.videoCodec), tracks.width, tracks.height, (tracks.alpha ? U" (alpha)" : U""));

		if (tracks.audioCodec == CODEC_UNKNOWN)
		{
			s += U", audio: none";
		}
		else
		{
			s += U", audio: {} {}ch {}Hz"_fmt(CodecName(tracks.audioCodec), tracks.channels, tracks.sampleRate);
		}

		return s;
	}
}
