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

# include "VideoReaderDetail.hpp"
# include <Siv3D/FileSystem.hpp>
# include <Siv3D/EngineLog.hpp>
# include <Siv3D/OpenCV_Bridge.hpp>

namespace s3d
{
	namespace
	{
		// OpenCV の VideoCapture による読み出し元 (MP4 など。OS の機能 / FFmpeg でデコードする)
		class OpenCVVideoSource final : public IVideoSource
		{
		public:

			~OpenCVVideoSource() override
			{
				m_capture.release();
			}

			[[nodiscard]]
			bool open(const FilePathView path)
			{
				if (not m_capture.open(path.narrow(), cv::CAP_ANY))
				{
					return false;
				}

				m_info.resolution	= Size{ static_cast<int32>(m_capture.get(cv::CAP_PROP_FRAME_WIDTH)), static_cast<int32>(m_capture.get(cv::CAP_PROP_FRAME_HEIGHT)) };
				m_info.fps			= m_capture.get(cv::CAP_PROP_FPS);
				m_info.frameCount	= static_cast<size_t>(static_cast<int32>(m_capture.get(cv::CAP_PROP_FRAME_COUNT)));

				return true;
			}

			[[nodiscard]]
			const Info& info() const noexcept override
			{
				return m_info;
			}

			[[nodiscard]]
			bool readFrame(Image& image) override
			{
				if (not m_capture.grab())
				{
					return false;
				}

				m_capture.retrieve(m_mat);
				OpenCV_Bridge::FromMatVec3b(m_mat, image, OverwriteAlpha::Yes);
				return true;
			}

			void seekFrame(const int32 frameIndex) override
			{
				m_capture.set(cv::CAP_PROP_POS_FRAMES, frameIndex);
			}

		private:

			cv::VideoCapture m_capture;

			cv::Mat m_mat;

			Info m_info;
		};
	}

	std::unique_ptr<IVideoSource> CreateOpenCVVideoSource(const FilePathView path)
	{
		auto source = std::make_unique<OpenCVVideoSource>();

		if (not source->open(path))
		{
			return nullptr;
		}

		return source;
	}

	VideoReader::VideoReaderDetail::VideoReaderDetail() {}

	VideoReader::VideoReaderDetail::~VideoReaderDetail()
	{
		close();
	}

	bool VideoReader::VideoReaderDetail::open(const FilePathView path)
	{
		LOG_SCOPED_TRACE(U"VideoReaderDetail::open()");

		close();

		// WebM は movie-player、それ以外 (と movie-player で開けなかった場合) は OpenCV で開く
	# if defined(SIV3D_WITH_MOVIEPLAYER)
		m_shared.source = CreateWebMVideoSource(path);
	# endif

		if (not m_shared.source)
		{
			m_shared.source = CreateOpenCVVideoSource(path);
		}

		if (not m_shared.source)
		{
			return false;
		}

		const IVideoSource::Info& info = m_shared.source->info();

		m_info =
		{
			.fullPath	= FileSystem::FullPath(path),
			.resolution	= info.resolution,
			.fps		= info.fps,
			.readPos	= 0,
			.frameCount	= info.frameCount,
			.isOpen		= true,
		};

		LOG_INFO(U"ℹ️ VideoReader: file `{0}` opened (resolution: {1}, fps: {2}, frameCount: {3})"_fmt(
			path, m_info.resolution, m_info.fps, m_info.frameCount));

		m_task = Async(&VideoReaderDetail::run, this);

		return true;
	}

	void VideoReader::VideoReaderDetail::close()
	{
		LOG_SCOPED_TRACE(U"VideoReaderDetail::close()");

		// スレッドを終了
		if (m_task.isValid())
		{
			{
				std::lock_guard guard(m_mutex);
				m_shared.stop = true;
			}

			// ブロック解除を通知
			m_cv.notify_one();

			m_task.get();
		}

		m_shared.source.reset();
		m_shared = {};
		m_info = {};
	}

	void VideoReader::VideoReaderDetail::run()
	{
		for (;;)
		{
			std::unique_lock ul(m_mutex);

			if (not m_shared.reachedEnd)
			{
				if (m_shared.source->readFrame(m_shared.frame.image))
				{
					m_shared.frame.index = m_shared.readPos++;
					//LOG_TEST(U"## info ## retrieved frame {} to buffer"_fmt(m_shared.frame.index));
				}
				else
				{
					m_shared.reachedEnd = true;
					//LOG_TEST(U"## info ## m_shared.reachedEnd = true;");
				}
			}

			m_shared.ready = true;
			//LOG_TEST(U"## info ## m_shared.ready = true;");

			ul.unlock();
			m_cv.notify_one();
			ul.lock();

			m_cv.wait(ul, [this]() { return (m_shared.stop) || (m_shared.ready == false); });

			// デコード処理のループを終了
			if (m_shared.stop)
			{
				//LOG_TEST(U"## info ## LOOP END");
				break;
			}

			//LOG_TEST(U"## info ## -----");
		}
	}
	
	bool VideoReader::VideoReaderDetail::isOpen() const noexcept
	{
		return m_info.isOpen;
	}

	bool VideoReader::VideoReaderDetail::readFrame(Image& image)
	{
		//LOG_SCOPED_TRACE(U"VideoReaderDetail::getFrame()");

		if (static_cast<int32>(m_info.frameCount) <= m_info.readPos)
		{
			return false;
		}

		{
			std::unique_lock ul(m_mutex);

			m_cv.wait(ul, [this]() { return m_shared.ready; });

			if (m_shared.frame.index == m_info.readPos)
			{
				//LOG_TEST(U"## info ## getFrmae(): targetBufferIndex {} found in buffer"_fmt(m_info.readPos));

				image.swap(m_shared.frame.image);
				++m_info.readPos;
				m_shared.ready = false;
				//LOG_TEST(U"## info ## m_shared.ready = false;");

				ul.unlock();
				m_cv.notify_one();
				return true;
			}
			else
			{
				//LOG_TEST(U"## info ## getFrmae(): targetBufferIndex {} not found in buffer"_fmt(m_info.readPos));

				m_shared.readPos = m_info.readPos;
				m_shared.source->seekFrame(m_shared.readPos);
				m_shared.reachedEnd = false;
				m_shared.ready = false;
				//LOG_TEST(U"## info ## m_shared.ready = false;");
				//LOG_TEST(U"## info ## REQUESTING frame {}"_fmt(m_shared.readPos));

				ul.unlock();
				m_cv.notify_one();
				ul.lock();
				m_cv.wait(ul, [this]() { return m_shared.ready; });

				//LOG_TEST(U"## info ## getFrmae(): targetBufferIndex {} found in buffer"_fmt(m_info.readPos));
				image.swap(m_shared.frame.image);
				++m_info.readPos;
				m_shared.ready = false;

				ul.unlock();
				m_cv.notify_one();
				return true;
			}
		}
	}

	const Size& VideoReader::VideoReaderDetail::getSize() const noexcept
	{
		return m_info.resolution;
	}

	double VideoReader::VideoReaderDetail::getFPS() const noexcept
	{
		return m_info.fps;
	}

	double VideoReader::VideoReaderDetail::getLengthSec() const
	{
		return (getFrameDeltaSec() * m_info.frameCount);
	}

	void VideoReader::VideoReaderDetail::setCurrentFrameIndex(const size_t index)
	{
		m_info.readPos = static_cast<int32>(Clamp<size_t>(index, 0, m_info.frameCount));
	}

	size_t VideoReader::VideoReaderDetail::getCurrentFrameIndex() const noexcept
	{
		return static_cast<size_t>(m_info.readPos);
	}

	double VideoReader::VideoReaderDetail::getPosSec() const
	{
		return (getProgress() * getLengthSec());
	}

	size_t VideoReader::VideoReaderDetail::getFrameCount() const noexcept
	{
		return m_info.frameCount;
	}

	double VideoReader::VideoReaderDetail::getFrameDeltaSec() const noexcept
	{
		return (m_info.fps ? (1.0 / m_info.fps) : (1.0 / 30.0));
	}

	double VideoReader::VideoReaderDetail::getProgress() const noexcept
	{
		return (static_cast<double>(m_info.readPos) / m_info.frameCount);
	}

	bool VideoReader::VideoReaderDetail::reachedEnd() const noexcept
	{
		return (m_info.readPos == static_cast<int32>(m_info.frameCount));
	}

	const FilePath& VideoReader::VideoReaderDetail::path() const noexcept
	{
		return m_info.fullPath;
	}
}
