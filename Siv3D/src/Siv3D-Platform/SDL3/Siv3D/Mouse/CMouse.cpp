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
# include <Siv3D/EngineLog.hpp>
# include <Siv3D/Window/IWindow.hpp>
# include <Siv3D/Common/Siv3DEngine.hpp>
# include "CMouse.hpp"

namespace s3d
{
	CMouse::CMouse()
	{

	}

	CMouse::~CMouse()
	{
		LOG_SCOPED_TRACE(U"CMouse::~CMouse()");
	}

	void CMouse::init()
	{
		LOG_SCOPED_TRACE(U"CMouse::init()");

		m_window = static_cast<SDL_Window*>(SIV3D_ENGINE(Window)->getHandle());

		m_buttonsInternal.fill(MouseButtonState::Released);
	}

	void CMouse::update()
	{
		{
			std::lock_guard lock{ m_buttonMutex };
			
			for (uint32 i = 0; i < InputState::MouseButtonCount; ++i)
			{
				auto& state = m_buttonsInternal[i];
				
				const bool pressed = (state == MouseButtonState::Pressed) || (state == MouseButtonState::Tapped);
				
				m_states[i].update(pressed);
				
				if (state == MouseButtonState::Tapped)
				{
					state = MouseButtonState::Released;
				}
			}
		}

		{
			std::lock_guard lock{ m_scrollMutex };
			
			m_scroll = std::exchange(m_scrollInternal, Vec2{ 0, 0 });
		}

		{
			m_allInputs.clear();

			for (uint32 i = 0; i < InputState::MouseButtonCount; ++i)
			{
				const auto& state = m_states[i];

				if (state.pressed() || state.up())
				{
					m_allInputs.emplace_back(InputDeviceType::Mouse, static_cast<uint8>(i));
				}
			}
		}
	}
    
    void CMouse::onMouseButtonUpdated(int32 button, bool pressed)
    {
        uint32 index;
        switch (button)
        {
            case SDL_BUTTON_LEFT:   index = 0; break;
            case SDL_BUTTON_RIGHT:  index = 1; break;
            case SDL_BUTTON_MIDDLE: index = 2; break;
            case SDL_BUTTON_X1:     index = 3; break;
            case SDL_BUTTON_X2:     index = 4; break;
            default: return;
        }
        
        std::lock_guard lock{ m_buttonMutex };
        
        if (pressed)
        {
            if (m_buttonsInternal[index] == MouseButtonState::Released)
            {
                m_buttonsInternal[index] = MouseButtonState::Pressed;
            }
        }
        else
        {
            if (m_buttonsInternal[index] == MouseButtonState::Pressed)
            {
                m_buttonsInternal[index] = MouseButtonState::Released;
            }
        }
    }
    
    void CMouse::onScroll(double x, double y)
    {
        std::lock_guard lock{ m_scrollMutex };
        m_scrollInternal.moveBy(x, y);
    }

	bool CMouse::down(const uint32 index) const
	{
		assert(index < InputState::MouseButtonCount);
		return m_states[index].down();
	}

	bool CMouse::pressed(const uint32 index) const
	{
		assert(index < InputState::MouseButtonCount);
		return m_states[index].pressed();
	}

	bool CMouse::up(const uint32 index) const
	{
		assert(index < InputState::MouseButtonCount);
		return m_states[index].up();
	}

	void CMouse::clearInput(const uint32 index)
	{
		assert(index < InputState::MouseButtonCount);
		m_states[index].clearInput();
	}

	bool CMouse::cleared(const uint32 index) const
	{
		assert(index < InputState::MouseButtonCount);
		return m_states[index].cleared();
	}

	Duration CMouse::pressedDuration(const uint32 index) const
	{
		assert(index < InputState::MouseButtonCount);
		return m_states[index].pressedDuration;
	}

	const Array<Input>& CMouse::getAllInput() const noexcept
	{
		return m_allInputs;
	}

	const Vec2& CMouse::wheel() const noexcept
	{
		return m_scroll;
	}
}
