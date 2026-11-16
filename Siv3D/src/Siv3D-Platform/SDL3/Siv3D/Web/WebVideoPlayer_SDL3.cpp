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
// Web (Emscripten) 版の動画再生: ブラウザの <video> 要素で WebMPlayer を実装する (VideoTexture が使う)。
//
// libvpx などのデコーダを wasm に移植するのではなく、デコード・音声の出力・同期をブラウザに任せる。
//   - 開く: 仮想ファイルシステムのファイルを Blob URL にして <video> に渡し、メタデータが揃うまで待つ (ASYNCIFY)
//   - フレーム: requestVideoFrameCallback で新しいフレームを検知し、fetchFrame で OffscreenCanvas に
//     drawImage → getImageData して Image に写す (RGBA のまま。並べ替えは要らない)
//   - 音声: <video> がそのまま鳴らす (Siv3D の Audio は通らないので audio() は空)。PlayAudio::No ならミュート
//   - 自動再生: ブラウザが play() を拒否したら (音付きでページを操作していないとき)、次のクリック・キー入力で再試行する
// WebM (VP8 / VP9 + Vorbis / Opus) のほか、ブラウザが再生できる形式 (MP4 など) も開ける。
//

# include <emscripten.h>
# include <Siv3D/FileSystem.hpp>
# include <Siv3D/EngineLog.hpp>
# include <Siv3D/VideoTexture/WebMPlayer.hpp>

namespace s3d
{
	namespace
	{
		// プレーヤーは int のハンドルで globalThis.__siv3dVideo に登録する

		// 開く: 戻り値 1 = 成功 (寸法と長さが確定)、0 = 失敗
		EM_ASYNC_JS(int, siv3d_video_open, (int handle, const char* cpath, int loop, int muted), {
			var P = globalThis.__siv3dVideo || (globalThis.__siv3dVideo = {});
			var path = UTF8ToString(cpath);
			var data;
			try {
				data = FS.readFile(path);
			} catch (e) {
				console.error('Siv3D video: cannot read', path, e);
				return 0;
			}
			var url = URL.createObjectURL(new Blob([data]));
			var v = document.createElement('video');
			v.preload = 'auto';
			v.playsInline = true;
			v.loop = !!loop;
			v.muted = !!muted;
			var st = { v: v, url: url, started: false, closed: false, newFrame: false, cap: null, ctx: null, retry: null };
			P[handle] = st;

			// 新しいフレームの検知。requestVideoFrameCallback が無いブラウザでは currentTime の変化で判定する
			if (typeof v.requestVideoFrameCallback === 'function') {
				var schedule = function() {
					if (st.closed) return;
					v.requestVideoFrameCallback(function() { st.newFrame = true; schedule(); });
				};
				schedule();
			} else {
				st.lastTime = -1;
			}

			return await new Promise(function(resolve) {
				var done = false;
				v.addEventListener('loadeddata', function() {
					if (!done) { done = true; st.newFrame = true; resolve(1); }
				});
				v.addEventListener('error', function() {
					if (!done) { done = true; console.error('Siv3D video: open failed', path, v.error); resolve(0); }
				});
				v.src = url;
			});
		});

		EM_JS(void, siv3d_video_close, (int handle), {
			var P = globalThis.__siv3dVideo;
			var st = P && P[handle];
			if (!st) return;
			st.closed = true;
			st.v.pause();
			st.v.removeAttribute('src');
			st.v.load();
			URL.revokeObjectURL(st.url);
			if (st.retry) {
				window.removeEventListener('pointerdown', st.retry);
				window.removeEventListener('keydown', st.retry);
			}
			delete P[handle];
		});

		EM_JS(void, siv3d_video_play, (int handle), {
			var st = globalThis.__siv3dVideo[handle];
			if (!st) return;
			if (st.started) {
				// 毎フレーム呼ばれる。終わった後にシークされた (ended でなくなった) ら再生を再開する
				if (st.v.paused && !st.v.ended && !st.retry) st.v.play().catch(function() {});
				return;
			}
			st.started = true;
			st.v.play().catch(function(e) {
				// 自動再生の制限: 次のユーザ操作で再試行する
				console.warn('Siv3D video: play() was rejected; waiting for a user gesture', e);
				st.retry = function() {
					window.removeEventListener('pointerdown', st.retry);
					window.removeEventListener('keydown', st.retry);
					st.retry = null;
					if (!st.closed) st.v.play().catch(function() {});
				};
				window.addEventListener('pointerdown', st.retry);
				window.addEventListener('keydown', st.retry);
			});
		});

		EM_JS(void, siv3d_video_seek, (int handle, double posSec), {
			var st = globalThis.__siv3dVideo[handle];
			if (!st) return;
			try { st.v.currentTime = posSec; } catch (e) {}
			// 一時停止中でもシーク先のフレームを取り込む
			st.newFrame = true;
		});

		EM_JS(int, siv3d_video_width, (int handle), {
			var st = globalThis.__siv3dVideo[handle];
			return st ? st.v.videoWidth : 0;
		});

		EM_JS(int, siv3d_video_height, (int handle), {
			var st = globalThis.__siv3dVideo[handle];
			return st ? st.v.videoHeight : 0;
		});

		EM_JS(double, siv3d_video_duration, (int handle), {
			var st = globalThis.__siv3dVideo[handle];
			if (!st) return 0;
			var d = st.v.duration;
			return isFinite(d) ? d : 0;
		});

		EM_JS(double, siv3d_video_position, (int handle), {
			var st = globalThis.__siv3dVideo[handle];
			return st ? st.v.currentTime : 0;
		});

		// 新しいフレームがあれば OffscreenCanvas に取り込み、寸法を返す (無ければ 0)。
		// 続けて siv3d_video_copy で Image のバッファに写す (間に C++ 側で Image の大きさを合わせるため 2 段階)
		EM_JS(int, siv3d_video_grab, (int handle, int* outW, int* outH), {
			var st = globalThis.__siv3dVideo[handle];
			if (!st) return 0;
			var v = st.v;
			if (st.lastTime !== undefined) {
				if (v.currentTime === st.lastTime && !st.newFrame) return 0;
				st.lastTime = v.currentTime;
			} else if (!st.newFrame) {
				return 0;
			}
			st.newFrame = false;
			var w = v.videoWidth, h = v.videoHeight;
			if (!w || !h || v.readyState < 2) return 0;
			if (!st.cap || st.cap.width !== w || st.cap.height !== h) {
				st.cap = new OffscreenCanvas(w, h);
				st.ctx = st.cap.getContext('2d', { willReadFrequently: true });
			}
			try {
				st.ctx.drawImage(v, 0, 0, w, h);
				st.img = st.ctx.getImageData(0, 0, w, h);
			} catch (e) {
				console.error('Siv3D video: frame grab failed', e);
				return 0;
			}
			HEAP32[outW >> 2] = w;
			HEAP32[outH >> 2] = h;
			return 1;
		});

		// 取り込んだフレーム (RGBA) を dest (幅 * 4 バイト刻み) に写す
		EM_JS(void, siv3d_video_copy, (int handle, void* dest), {
			var st = globalThis.__siv3dVideo[handle];
			var img = st && st.img;
			if (!img) return;
			HEAPU8.set(img.data, dest);
			st.img = null;
		});
	}

	class WebMPlayer::WebMPlayerDetail
	{
	public:

		WebMPlayerDetail()
			: m_handle{ NextHandle() } {}

		~WebMPlayerDetail()
		{
			siv3d_video_close(m_handle);
		}

		bool open(const FilePathView path, const bool loop, const bool playAudio)
		{
			const FilePath fullPath = FileSystem::FullPath(path);

			if (not siv3d_video_open(m_handle, fullPath.toUTF8().c_str(), loop, (not playAudio)))
			{
				LOG_FAIL(U"❌ VideoTexture (Web): failed to open `{}`"_fmt(path));
				return false;
			}

			LOG_INFO(U"🎬 VideoTexture (Web): `{}` ({}x{}, {:.2f} s) via <video>"_fmt(path, siv3d_video_width(m_handle), siv3d_video_height(m_handle), siv3d_video_duration(m_handle)));
			return true;
		}

		Size size() const noexcept
		{
			return{ siv3d_video_width(m_handle), siv3d_video_height(m_handle) };
		}

		double lengthSec() const
		{
			return siv3d_video_duration(m_handle);
		}

		double posSec() const
		{
			return siv3d_video_position(m_handle);
		}

		void play()
		{
			siv3d_video_play(m_handle);
		}

		void seek(const double posSec)
		{
			siv3d_video_seek(m_handle, posSec);
		}

		bool fetchFrame(Image& image)
		{
			int w = 0, h = 0;

			if (not siv3d_video_grab(m_handle, &w, &h))
			{
				return false;
			}

			if (image.size() != Size{ w, h })
			{
				image.resize(w, h);
			}

			siv3d_video_copy(m_handle, image.data());
			return true;
		}

		const Audio& audio() const noexcept
		{
			return m_audio;
		}

	private:

		static int NextHandle()
		{
			static int next = 1;
			return next++;
		}

		int m_handle = 0;

		// 音声は <video> が直接鳴らすので、Siv3D の Audio は空
		Audio m_audio;
	};

	std::unique_ptr<WebMPlayer> WebMPlayer::Create(const FilePathView path, const bool loop, const bool playAudio)
	{
		auto detail = std::make_unique<WebMPlayerDetail>();

		if (not detail->open(path, loop, playAudio))
		{
			return nullptr;
		}

		return std::unique_ptr<WebMPlayer>(new WebMPlayer(std::move(detail)));
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
