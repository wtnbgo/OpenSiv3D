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

# include <Siv3D/Error.hpp>
# include <Siv3D/EngineLog.hpp>
# include <Siv3D/FormatLiteral.hpp>
# include <Siv3D/Utility.hpp>
# include <Siv3D/UserAction.hpp>
# include <Siv3D/Scene.hpp>
# include <Siv3D/Monitor.hpp>
# include <Siv3D/MonitorInfo.hpp>
# include <Siv3D/Unicode.hpp>
# include <Siv3D/FileSystem.hpp>
# include <Siv3D/Renderer/IRenderer.hpp>
# include <Siv3D/Profiler/IProfiler.hpp>
# include <Siv3D/UserAction/IUserAction.hpp>
# include <Siv3D/Common/Siv3DEngine.hpp>
# include "CWindow.hpp"
# include <Siv3D/Common/OpenGL.hpp>
# include "../Mouse/CMouse.hpp"
# include "../TextInput/CTextInput.hpp"

namespace s3d::detail
{
	// アプリの既定のウィンドウタイトル (UTF-8)。siv3d_add_app がアプリ側に生成するソースが、
	// main の前 (静的初期化) に siv3d_set_app_default_title() で siv3d-app.json の "name" を設定する
	static const char* g_appDefaultTitleUTF8 = nullptr;
}

extern "C" void siv3d_set_app_default_title(const char* titleUTF8)
{
	s3d::detail::g_appDefaultTitleUTF8 = titleUTF8;
}

namespace s3d
{
	CWindow::CWindow()
	{
		if (detail::g_appDefaultTitleUTF8 && *detail::g_appDefaultTitleUTF8)
		{
			m_title = Unicode::FromUTF8(detail::g_appDefaultTitleUTF8);
			m_actualTitle = (SIV3D_BUILD(DEBUG) ? (m_title + U" (Debug Build)") : m_title);
		}
	}

	CWindow::~CWindow()
	{
		LOG_SCOPED_TRACE(U"CWindow::~CWindow()");
		
		if (m_window)
		{
			SDL_DestroyWindow(m_window);
		}
		SDL_Quit();
	}

	void CWindow::init()
	{
		LOG_SCOPED_TRACE(U"CWindow::init()");

	# if SIV3D_PLATFORM(MACOS) && defined(SIV3D_ANGLE_GLES_LIB) && defined(SIV3D_ANGLE_EGL_LIB)
		// macOS には OpenGL ES が無いので、.app の Contents/Frameworks に同梱した
		// ANGLE (Metal バックエンド) を使う。SDL は既定でライブラリ名だけで dlopen するため、
		// バンドル内の絶対パスをヒントで渡す (ウィンドウ / コンテキスト生成前に必要)。
		// (SDL_GetBasePath() は .app/Contents/Resources/ を返すが、Resources が無いと "../Frameworks" を解決できないため
		//  実行ファイルのディレクトリ (.app/Contents/MacOS/) を基準にする)
		{
			const std::string base = FileSystem::ParentPath(FileSystem::ModulePath()).toUTF8();

			for (const std::string dir : { (base + "../Frameworks/"), base })
			{
				const std::string gles = (dir + SIV3D_ANGLE_GLES_LIB);
				const std::string egl = (dir + SIV3D_ANGLE_EGL_LIB);

				if (SDL_GetPathInfo(gles.c_str(), nullptr) && SDL_GetPathInfo(egl.c_str(), nullptr))
				{
					SDL_SetHint(SDL_HINT_OPENGL_LIBRARY, gles.c_str());
					SDL_SetHint(SDL_HINT_EGL_LIBRARY, egl.c_str());
					LOG_INFO(U"ANGLE: {} / {}"_fmt(Unicode::Widen(gles), Unicode::Widen(egl)));
					break;
				}
			}
		}
	# endif

		if (not SDL_Init(SDL_INIT_VIDEO))
		{
			throw EngineError(U"SDL_Init() failed: {}"_fmt(Unicode::Widen(SDL_GetError())));
		}

		// OpenGL ES 3.0 (SDL3 版では GLES のみ使用)
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
		SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
		if (SIV3D_BUILD(DEBUG))
		{
			SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, SDL_GL_CONTEXT_DEBUG_FLAG);
		}
		
		// Create Window
		SDL_WindowFlags windowFlags = (SDL_WINDOW_OPENGL | SDL_WINDOW_HIDDEN);
	# if SIV3D_PLATFORM(MACOS)
		// Retina: フレームバッファを物理ピクセル解像度にする (virtualSize はポイント単位のまま)
		windowFlags |= SDL_WINDOW_HIGH_PIXEL_DENSITY;
	# endif
		m_window = SDL_CreateWindow(m_actualTitle.narrow().c_str(),
									m_state.virtualSize.x, m_state.virtualSize.y,
									windowFlags);
		
		if (!m_window)
		{
			throw EngineError(U"SDL_CreateWindow() failed: {}"_fmt(Unicode::Widen(SDL_GetError())));
		}
		
		// Set minimum size
		SDL_SetWindowMinimumSize(m_window, m_state.minFrameBufferSize.x, m_state.minFrameBufferSize.y);

		// Create Context
		SDL_GLContext context = SDL_GL_CreateContext(m_window);
		if (!context)
		{
			throw EngineError(U"SDL_GL_CreateContext() failed: {}"_fmt(Unicode::Widen(SDL_GetError())));
		}
		
		SDL_GL_MakeCurrent(m_window, context);

		// Initialize glad (GLES)
		if (not gladLoadGLES2(reinterpret_cast<GLADloadfunc>(SDL_GL_GetProcAddress)))
		{
			throw EngineError(U"gladLoadGLES2() failed");
		}
		
		// Show window
		SDL_ShowWindow(m_window);

		updateState();
	}

	void CWindow::update()
	{
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN || event.type == SDL_EVENT_MOUSE_BUTTON_UP)
			{
				static_cast<CMouse*>(SIV3D_ENGINE(Mouse))->onMouseButtonUpdated(event.button.button, (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN));
			}
			else if (event.type == SDL_EVENT_MOUSE_WHEEL)
			{
				static_cast<CMouse*>(SIV3D_ENGINE(Mouse))->onScroll(event.wheel.x, event.wheel.y);
			}
			else if (event.type == SDL_EVENT_TEXT_INPUT)
			{
				static_cast<CTextInput*>(SIV3D_ENGINE(TextInput))->onTextInput(event.text.text);
			}
			else if (event.type == SDL_EVENT_TEXT_EDITING)
			{
				static_cast<CTextInput*>(SIV3D_ENGINE(TextInput))->onTextEditing(event.edit.text, event.edit.start, event.edit.length);
			}

		# if SIV3D_PLATFORM(IOS)
			// iOS: 終了要求 (シグナルなどから SDL が送る SDL_EVENT_QUIT) では終了しない。
			// iOS のアプリは自分で終了しないのが原則で、Main() を抜けても OS はプロセスを終了させないため、
			// 受け付けると黒い画面のまま止まってしまう。アプリの終了は OS (ユーザのスワイプなど) に任せる
			if ((event.type == SDL_EVENT_QUIT) || (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED))
			{
				LOG_INFO(U"CWindow::update(): quit request ignored on iOS");
			}
		# else
			if (event.type == SDL_EVENT_QUIT)
			{
				SIV3D_ENGINE(UserAction)->reportUserActions(UserAction::CloseButtonClicked);
			}
			else if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(m_window))
			{
				SIV3D_ENGINE(UserAction)->reportUserActions(UserAction::CloseButtonClicked);
			}
		# endif
		# if defined(__ANDROID__)
			else if ((event.type == SDL_EVENT_KEY_DOWN) && (event.key.key == SDLK_AC_BACK) && (not event.key.repeat))
			{
				// Android の戻るボタン: ウィンドウの閉じるボタンと同じ扱いにする (既定では System::Update() が false を返して終了。
				// 終了させたくない場合は System::SetTerminationTriggers から UserAction::CloseButtonClicked を外す)
				SIV3D_ENGINE(UserAction)->reportUserActions(UserAction::CloseButtonClicked);
			}
		# endif
		}
		
		updateState();

		if constexpr (SIV3D_BUILD(DEBUG))
		{
			setWindowTitle(m_title);
		}
	}

	void CWindow::setWindowTitle(const String& title)
	{
		String newActualTitle = title;
		
		if constexpr (SIV3D_BUILD(DEBUG))
		{
			const String statistics = SIV3D_ENGINE(Profiler)->getSimpleStatistics();
			newActualTitle += U" (Debug Build) | ";
			newActualTitle += statistics;
		}

		if (m_actualTitle != newActualTitle)
		{
			SDL_SetWindowTitle(m_window, newActualTitle.narrow().c_str());
			m_actualTitle.swap(newActualTitle);
		}

		m_title = title;
	}

	const String& CWindow::getWindowTitle() const noexcept
	{
		return m_title;
	}

	void* CWindow::getHandle() const noexcept
	{
		return m_window;
	}

	const WindowState& CWindow::getState() const noexcept
	{
		return m_state;
	}

	void CWindow::setStyle(const WindowStyle style)
	{
		LOG_TRACE(U"CWindow::setStyle(style = {})"_fmt(FromEnum(style)));
		
		if (m_state.fullscreen)
		{
			m_state.style = style;
			return;
		}

		if (m_state.style == style)
		{
			return;
		}

		if (style == WindowStyle::Fixed)
		{
			SDL_SetWindowResizable(m_window, false);
			SDL_SetWindowBordered(m_window, true);
		}
		else if (style == WindowStyle::Sizable)
		{
			SDL_SetWindowResizable(m_window, true);
			SDL_SetWindowBordered(m_window, true);
		}
		else if (style == WindowStyle::Frameless)
		{
			SDL_SetWindowResizable(m_window, false);
			SDL_SetWindowBordered(m_window, false);
		}
		
		m_state.style = style;
	}

	void CWindow::setPos(const Point& pos)
	{
		LOG_TRACE(U"CWindow::setPos(pos = {})"_fmt(pos));

		if (m_state.fullscreen)
		{
			return;
		}
		
		SDL_SetWindowPosition(m_window, pos.x, pos.y + m_state.titleBarHeight);
	}

	void CWindow::maximize()
	{
		LOG_TRACE(U"CWindow::maximize()");

		if (m_state.style == WindowStyle::Fixed)
		{
			return;
		}
		
		SDL_MaximizeWindow(m_window);
	}

	void CWindow::restore()
	{
		LOG_TRACE(U"CWindow::restore()");
		SDL_RestoreWindow(m_window);
	}

	void CWindow::minimize()
	{
		LOG_TRACE(U"CWindow::minimize()");
		SDL_MinimizeWindow(m_window);
	}

	bool CWindow::resizeByVirtualSize(const Size& virtualSize)
	{
		LOG_TRACE(U"CWindow::resizeByVirtualSize({})"_fmt(virtualSize));

		SDL_SetWindowSize(m_window, virtualSize.x, virtualSize.y);
		return true;
	}

	bool CWindow::resizeByFrameBufferSize(const Size& frameBufferSize)
	{
		LOG_TRACE(U"CWindow::resizeByFrameBufferSize({})"_fmt(frameBufferSize));
		
		const double scaling = m_state.scaling;
		const Size newVirtualSize = (frameBufferSize * (1.0 / scaling)).asPoint();
		return resizeByVirtualSize(newVirtualSize);
	}

	void CWindow::setMinimumFrameBufferSize(const Size& size)
	{
		LOG_TRACE(U"CWindow::setMinimumFrameBufferSize({})"_fmt(size));
		
		m_state.minFrameBufferSize = size;
		SDL_SetWindowMinimumSize(m_window, size.x, size.y);
	}
	
	void CWindow::setFullscreen(const bool fullscreen, size_t monitorIndex)
	{
		LOG_TRACE(U"CWindow::setFullscreen(fullscreen = {}, monitorIndex = {})"_fmt(fullscreen, monitorIndex));

		if (fullscreen == m_state.fullscreen)
		{
			return;
		}
		
		if (m_state.fullscreen == false) // 現在ウィンドウモード
		{
			// Store current window rect
			int x, y, w, h;
			SDL_GetWindowPosition(m_window, &x, &y);
			SDL_GetWindowSize(m_window, &w, &h);
			m_storedWindowRect = Rect(x, y, w, h);

			SDL_SetWindowFullscreen(m_window, true);
		}
		else
		{
			SDL_SetWindowFullscreen(m_window, false);
			SDL_SetWindowPosition(m_window, m_storedWindowRect.x, m_storedWindowRect.y);
			SDL_SetWindowSize(m_window, m_storedWindowRect.w, m_storedWindowRect.h);
		}
		
		m_state.fullscreen = fullscreen;

		if (Scene::GetResizeMode() != ResizeMode::Keep)
		{
			SIV3D_ENGINE(Renderer)->updateSceneSize();
		}
	}

	void CWindow::setToggleFullscreenEnabled(bool) {}
	
	bool CWindow::isToggleFullscreenEnabled() const
	{
		return false;
	}

	void CWindow::setTaskbarProgressBar(double)
	{
		// do nothing
	}

	void CWindow::updateState()
	{
		// frameBufferSize
		int fw, fh;
		SDL_GetWindowSizeInPixels(m_window, &fw, &fh);
		m_state.frameBufferSize.set(fw, fh);

		// scaling
	# if SIV3D_PLATFORM(MACOS)
		// macOS の Retina はコンテンツスケールではなくピクセル密度で表現されるため、両者を含むウィンドウのスケールを使う
		float scale = SDL_GetWindowDisplayScale(m_window);
	# else
		SDL_DisplayID displayID = SDL_GetDisplayForWindow(m_window);
		float scale = SDL_GetDisplayContentScale(displayID);
	# endif
		if (scale == 0.0f) scale = 1.0f;
		m_state.scaling = scale;
		
		// titleBarHeight
		// [Siv3D ToDo]
		
		// bounds
		int windowPosX, windowPosY, windowSizeX, windowSizeY;
		SDL_GetWindowPosition(m_window, &windowPosX, &windowPosY);
		SDL_GetWindowSize(m_window, &windowSizeX, &windowSizeY);
		
		m_state.bounds.pos = Size(windowPosX, (windowPosY - m_state.titleBarHeight));
		m_state.bounds.size = Size(windowSizeX, (windowSizeY + m_state.titleBarHeight));
				
		// minimized
		Uint32 flags = SDL_GetWindowFlags(m_window);
		m_state.minimized = (flags & SDL_WINDOW_MINIMIZED) != 0;
		
		// maximized
		m_state.maximized = (flags & SDL_WINDOW_MAXIMIZED) != 0;
		
		// focused
		m_state.focused = (flags & SDL_WINDOW_INPUT_FOCUS) != 0;

		// sizeMove
		m_state.sizeMove = false; // [Siv3D ToDo]
	}
}
