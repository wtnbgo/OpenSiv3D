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

# include <Siv3D/Common.hpp>
# include <Siv3D/Image.hpp>
# include <Siv3D/WindowState.hpp>
# include <Siv3D/Window/IWindow.hpp>
# include <Siv3D/Common/Siv3DEngine.hpp>
# include <Siv3D/Renderer/IRenderer.hpp>
# include <Siv3D/EngineLog.hpp>
# include "CCursor.hpp"

namespace s3d
{
	namespace detail
	{
		[[nodiscard]]
		static Point GetScreenPos()
		{
			float x, y;
			SDL_GetGlobalMouseState(&x, &y);
			return Point(static_cast<int>(x), static_cast<int>(y));
		}
	
		[[nodiscard]]
		static Vec2 GetClientCursorPos(SDL_Window* window)
		{
			float clientX, clientY;
			SDL_GetMouseState(&clientX, &clientY);
			return{ clientX, clientY };
		}

		[[nodiscard]]
		static SDL_Cursor* CreateCursor(const Image& image, const Point hotSpot)
		{
			const int width = image.width();
			const int height = image.height();
			const void* pixels = image.data();
			const int pitch = image.stride();
			
			SDL_Surface* surface = SDL_CreateSurfaceFrom(width, height, SDL_PIXELFORMAT_RGBA32, (void*)pixels, pitch);
			
			if (!surface)
			{
				return nullptr;
			}

			SDL_Cursor* cursor = SDL_CreateColorCursor(surface, hotSpot.x, hotSpot.y);
			SDL_DestroySurface(surface);
			
			return cursor;
		}
	}

	CCursor::CCursor()
		: m_systemCursors{}
	{

	}

	CCursor::~CCursor()
	{
		LOG_SCOPED_TRACE(U"CCursor::~CCursor()");

		m_customCursors.clear();

		for (auto& systemCursor : m_systemCursors)
		{
			if (systemCursor)
			{
				SDL_DestroyCursor(systemCursor);
				systemCursor = nullptr;
			}
		}
	}

	void CCursor::init()
	{
		LOG_SCOPED_TRACE(U"CCursor::init()");

		m_window = static_cast<SDL_Window*>(SIV3D_ENGINE(Window)->getHandle());

		{
			const Vec2 clientPos = detail::GetClientCursorPos(m_window);
			const Point screenPos = detail::GetScreenPos();
			
			const Vec2 frameBufferSize = SIV3D_ENGINE(Window)->getState().frameBufferSize;
			const Vec2 virtualSize = SIV3D_ENGINE(Window)->getState().virtualSize;
			const double uiScaling = (virtualSize.x > 0) ? (frameBufferSize.x / virtualSize.x) : 1.0;
			
			m_state.update(clientPos.asPoint(), clientPos / uiScaling, screenPos);
		}

		m_systemCursors[FromEnum(CursorStyle::Arrow)]			= SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_DEFAULT);
		m_systemCursors[FromEnum(CursorStyle::IBeam)]			= SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_TEXT);
		m_systemCursors[FromEnum(CursorStyle::Cross)]			= SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_CROSSHAIR);
		m_systemCursors[FromEnum(CursorStyle::Hand)]			= SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_POINTER);
		m_systemCursors[FromEnum(CursorStyle::NotAllowed)]		= SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NOT_ALLOWED);
		m_systemCursors[FromEnum(CursorStyle::ResizeUpDown)]	= SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NS_RESIZE);
		m_systemCursors[FromEnum(CursorStyle::ResizeLeftRight)]	= SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_EW_RESIZE);
        m_systemCursors[FromEnum(CursorStyle::ResizeNESW)]		= SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NESW_RESIZE);
        m_systemCursors[FromEnum(CursorStyle::ResizeNWSE)]		= SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NWSE_RESIZE);
        m_systemCursors[FromEnum(CursorStyle::ResizeAll)]		= SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_MOVE);
		m_systemCursors[FromEnum(CursorStyle::Hidden)]			= nullptr;

		m_defaultCursor = m_systemCursors[FromEnum(CursorStyle::Arrow)];
		m_currentCursor = m_defaultCursor;
		
		SDL_SetCursor(m_currentCursor);
	}

	bool CCursor::update()
	{
		if (!m_window)
		{
			return true;
		}

		{
			const Vec2 clientPos = detail::GetClientCursorPos(m_window);
			const Point screenPos = detail::GetScreenPos();
			
			const Vec2 frameBufferSize = SIV3D_ENGINE(Window)->getState().frameBufferSize;
			const Vec2 virtualSize = SIV3D_ENGINE(Window)->getState().virtualSize;
			const double uiScaling = (virtualSize.x > 0) ? (frameBufferSize.x / virtualSize.x) : 1.0;
			
			m_state.update(clientPos.asPoint(), clientPos / uiScaling, screenPos);
		}
		
		if (m_requestedCursor != m_currentCursor)
		{
			m_currentCursor = m_requestedCursor;
			
			if (m_currentCursor)
			{
				SDL_SetCursor(m_currentCursor);
				if (!SDL_CursorVisible())
				{
					SDL_ShowCursor();
				}
			}
			else
			{
				SDL_HideCursor();
			}
		}

		return true;
	}

	const CursorState& CCursor::getState() const noexcept
	{
		return m_state;
	}

	void CCursor::setPos(const Point pos)
	{
		if (!m_window)
		{
			return;
		}
		
		SDL_WarpMouseInWindow(m_window, static_cast<float>(pos.x), static_cast<float>(pos.y));
		
		m_state.vec2.update(Vec2{ pos });
		m_state.raw.update(pos);
	}

	const Mat3x2& CCursor::getLocalTransform() const noexcept
	{
		return m_transformLocal;
	}

	const Mat3x2& CCursor::getCameraTransform() const noexcept
	{
		return m_transformCamera;
	}

	const Mat3x2& CCursor::getScreenTransform() const noexcept
	{
		return m_transformScreen;
	}

	void CCursor::setLocalTransform(const Mat3x2& matrix)
	{
		if (m_transformLocal == matrix)
		{
			return;
		}

		m_transformLocal = matrix;
		m_transformAll = (m_transformLocal * m_transformCamera * m_transformScreen);
		m_transformAllInv = m_transformAll.inverse();
	}

	void CCursor::setCameraTransform(const Mat3x2& matrix)
	{
		if (m_transformCamera == matrix)
		{
			return;
		}

		m_transformCamera = matrix;
		m_transformAll = (m_transformLocal * m_transformCamera * m_transformScreen);
		m_transformAllInv = m_transformAll.inverse();
	}

	void CCursor::setScreenTransform(const Mat3x2& matrix)
	{
		if (m_transformScreen == matrix)
		{
			return;
		}

		m_transformScreen = matrix;
		m_transformAll = (m_transformLocal * m_transformCamera * m_transformScreen);
		m_transformAllInv = m_transformAll.inverse();
	}

	bool CCursor::isClippedToWindow() const noexcept
	{
		return m_clipToWindow;
	}

	void CCursor::clipToWindow(const bool clip)
	{
		if (m_clipToWindow == clip)
		{
			return;
		}

		m_clipToWindow = clip;
		
		SDL_SetWindowMouseGrab(m_window, clip);
	}

	void CCursor::requestStyle(const CursorStyle style)
	{
		m_requestedCursor = m_systemCursors[FromEnum(style)];
	}

	void CCursor::setDefaultStyle(const CursorStyle style)
	{
		m_defaultCursor = m_systemCursors[FromEnum(style)];
	}

	bool CCursor::registerCursor(const StringView name, const Image& image, const Point hotSpot)
	{
		if (m_customCursors.contains(name))
		{
			return false;
		}

		if (SDL_Cursor* cursor = detail::CreateCursor(image, hotSpot))
		{
			m_customCursors.emplace(name, unique_resource<SDL_Cursor*, decltype(&CursorDeleter)>(cursor, CursorDeleter));
			return true;
		}
		else
		{
			return false;
		}
	}

	void CCursor::requestStyle(const StringView name)
	{
		if (auto it = m_customCursors.find(name); it != m_customCursors.end())
		{
			m_requestedCursor = it->second.get();
		}
		else
		{
			m_requestedCursor = m_defaultCursor;
		}
	}

	void CCursor::setCapture(const bool captured) noexcept
	{
		if (m_captured == captured)
		{
			return;
		}
		
		m_captured = captured;
		
		if (m_captured)
		{
			SDL_CaptureMouse(true);
		}
		else
		{
			SDL_CaptureMouse(false);
		}
	}

	bool CCursor::isCaptured() const noexcept
	{
		return m_captured;
	}
}
