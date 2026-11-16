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
# include <Siv3D/Unicode.hpp>
# include <Siv3D/Char.hpp>
# include <Siv3D/UserAction.hpp>
# include <Siv3D/Window/IWindow.hpp>
# include <Siv3D/UserAction/IUserAction.hpp>
# include <Siv3D/Common/Siv3DEngine.hpp>
# include <Siv3D/Keyboard/FallbackKeyName.hpp>
# include "CKeyboard.hpp"

namespace s3d
{
	namespace detail
	{
		constexpr std::pair<uint8, SDL_Scancode> KeyConversionTable[]
		{
			{ 0x08, SDL_SCANCODE_BACKSPACE },
			{ 0x09, SDL_SCANCODE_TAB },
			{ 0x0D, SDL_SCANCODE_RETURN },
			{ 0x10, SDL_SCANCODE_UNKNOWN }, // Shift (Handled manually)
			{ 0x11, SDL_SCANCODE_UNKNOWN }, // Control (Handled manually)
			{ 0x12, SDL_SCANCODE_UNKNOWN }, // Alt (Handled manually)
			{ 0x13, SDL_SCANCODE_PAUSE },
			{ 0x1B, SDL_SCANCODE_ESCAPE },
			{ 0x20, SDL_SCANCODE_SPACE },
			{ 0x21, SDL_SCANCODE_PAGEUP },
			{ 0x22, SDL_SCANCODE_PAGEDOWN },
			{ 0x23, SDL_SCANCODE_END },
			{ 0x24, SDL_SCANCODE_HOME },
			{ 0x25, SDL_SCANCODE_LEFT },
			{ 0x26, SDL_SCANCODE_UP },
			{ 0x27, SDL_SCANCODE_RIGHT },
			{ 0x28, SDL_SCANCODE_DOWN },
			{ 0x2C, SDL_SCANCODE_PRINTSCREEN },
			{ 0x2D, SDL_SCANCODE_INSERT },
			{ 0x2E, SDL_SCANCODE_DELETE },
			{ 0x30, SDL_SCANCODE_0 },
			{ 0x31, SDL_SCANCODE_1 },
			{ 0x32, SDL_SCANCODE_2 },
			{ 0x33, SDL_SCANCODE_3 },
			{ 0x34, SDL_SCANCODE_4 },
			{ 0x35, SDL_SCANCODE_5 },
			{ 0x36, SDL_SCANCODE_6 },
			{ 0x37, SDL_SCANCODE_7 },
			{ 0x38, SDL_SCANCODE_8 },
			{ 0x39, SDL_SCANCODE_9 },
			{ 0x41, SDL_SCANCODE_A },
			{ 0x42, SDL_SCANCODE_B },
			{ 0x43, SDL_SCANCODE_C },
			{ 0x44, SDL_SCANCODE_D },
			{ 0x45, SDL_SCANCODE_E },
			{ 0x46, SDL_SCANCODE_F },
			{ 0x47, SDL_SCANCODE_G },
			{ 0x48, SDL_SCANCODE_H },
			{ 0x49, SDL_SCANCODE_I },
			{ 0x4A, SDL_SCANCODE_J },
			{ 0x4B, SDL_SCANCODE_K },
			{ 0x4C, SDL_SCANCODE_L },
			{ 0x4D, SDL_SCANCODE_M },
			{ 0x4E, SDL_SCANCODE_N },
			{ 0x4F, SDL_SCANCODE_O },
			{ 0x50, SDL_SCANCODE_P },
			{ 0x51, SDL_SCANCODE_Q },
			{ 0x52, SDL_SCANCODE_R },
			{ 0x53, SDL_SCANCODE_S },
			{ 0x54, SDL_SCANCODE_T },
			{ 0x55, SDL_SCANCODE_U },
			{ 0x56, SDL_SCANCODE_V },
			{ 0x57, SDL_SCANCODE_W },
			{ 0x58, SDL_SCANCODE_X },
			{ 0x59, SDL_SCANCODE_Y },
			{ 0x5A, SDL_SCANCODE_Z },
			{ 0x60, SDL_SCANCODE_KP_0 },
			{ 0x61, SDL_SCANCODE_KP_1 },
			{ 0x62, SDL_SCANCODE_KP_2 },
			{ 0x63, SDL_SCANCODE_KP_3 },
			{ 0x64, SDL_SCANCODE_KP_4 },
			{ 0x65, SDL_SCANCODE_KP_5 },
			{ 0x66, SDL_SCANCODE_KP_6 },
			{ 0x67, SDL_SCANCODE_KP_7 },
			{ 0x68, SDL_SCANCODE_KP_8 },
			{ 0x69, SDL_SCANCODE_KP_9 },
			{ 0x6A, SDL_SCANCODE_KP_MULTIPLY },
			{ 0x6B, SDL_SCANCODE_KP_PLUS },
			{ 0x6C, SDL_SCANCODE_KP_ENTER },
			{ 0x6D, SDL_SCANCODE_KP_MINUS },
			{ 0x6E, SDL_SCANCODE_KP_PERIOD },
			{ 0x6F, SDL_SCANCODE_KP_DIVIDE },
			{ 0x70, SDL_SCANCODE_F1 },
			{ 0x71, SDL_SCANCODE_F2 },
			{ 0x72, SDL_SCANCODE_F3 },
			{ 0x73, SDL_SCANCODE_F4 },
			{ 0x74, SDL_SCANCODE_F5 },
			{ 0x75, SDL_SCANCODE_F6 },
			{ 0x76, SDL_SCANCODE_F7 },
			{ 0x77, SDL_SCANCODE_F8 },
			{ 0x78, SDL_SCANCODE_F9 },
			{ 0x79, SDL_SCANCODE_F10 },
			{ 0x7A, SDL_SCANCODE_F11 },
			{ 0x7B, SDL_SCANCODE_F12 },
			{ 0x7C, SDL_SCANCODE_F13 },
			{ 0x7D, SDL_SCANCODE_F14 },
			{ 0x7E, SDL_SCANCODE_F15 },
			{ 0x7F, SDL_SCANCODE_F16 },
			{ 0x80, SDL_SCANCODE_F17 },
			{ 0x81, SDL_SCANCODE_F18 },
			{ 0x82, SDL_SCANCODE_F19 },
			{ 0x83, SDL_SCANCODE_F20 },
			{ 0x84, SDL_SCANCODE_F21 },
			{ 0x85, SDL_SCANCODE_F22 },
			{ 0x86, SDL_SCANCODE_F23 },
			{ 0x87, SDL_SCANCODE_F24 },
			{ 0x90, SDL_SCANCODE_NUMLOCKCLEAR },
			{ 0x91, SDL_SCANCODE_SCROLLLOCK },
			{ 0xA0, SDL_SCANCODE_LSHIFT },
			{ 0xA1, SDL_SCANCODE_RSHIFT },
			{ 0xA2, SDL_SCANCODE_LCTRL },
			{ 0xA3, SDL_SCANCODE_RCTRL },
			{ 0xA4, SDL_SCANCODE_LALT },
			{ 0xA5, SDL_SCANCODE_RALT },
			{ 0xBA, SDL_SCANCODE_SEMICOLON },
			{ 0xBB, SDL_SCANCODE_EQUALS },
			{ 0xBC, SDL_SCANCODE_COMMA },
			{ 0xBD, SDL_SCANCODE_MINUS },
			{ 0xBE, SDL_SCANCODE_PERIOD },
			{ 0xBF, SDL_SCANCODE_SLASH },
			{ 0xC0, SDL_SCANCODE_GRAVE },
			{ 0xDB, SDL_SCANCODE_LEFTBRACKET },
			{ 0xDC, SDL_SCANCODE_BACKSLASH },
			{ 0xDD, SDL_SCANCODE_RIGHTBRACKET },
			{ 0xDE, SDL_SCANCODE_APOSTROPHE },
		};
        
        static String GetKeyName(uint32 index, SDL_Scancode scancode)
        {
            if (scancode == SDL_SCANCODE_UNKNOWN)
            {
                return String(FallbackKeyNames[index]);
            }
            
            const char* name = SDL_GetScancodeName(scancode);
            if (name && *name)
            {
                return Unicode::Widen(name);
            }
            
            return String(FallbackKeyNames[index]);
        }
	}

	CKeyboard::CKeyboard()
	{

	}

	CKeyboard::~CKeyboard()
	{
		LOG_SCOPED_TRACE(U"CKeyboard::~CKeyboard()");
	}

	void CKeyboard::init()
	{
		LOG_SCOPED_TRACE(U"CKeyboard::init()");

		m_window = static_cast<SDL_Window*>(SIV3D_ENGINE(Window)->getHandle());
		
		for (auto [index, scancode] : detail::KeyConversionTable)
		{
			m_names[index] = detail::GetKeyName(index, scancode);
		}
		
		for (size_t i = 0; i < InputState::KeyCount; ++i)
		{
			auto& name = m_names[i];
			
			if (not name)
			{
				name = U"{:#04x}"_fmt(i);
			}
			else if (name.size() == 1 && IsLower(name.front()))
			{
				name.uppercase();
			}
		}
	}

	void CKeyboard::onKeyDown(const SDL_Scancode scancode) noexcept
	{
		if ((SDL_SCANCODE_UNKNOWN < scancode) && (scancode < SDL_SCANCODE_COUNT))
		{
			m_downSinceUpdate[scancode] = true;
		}
	}

	void CKeyboard::update()
	{
		if (not m_window)
		{
			return;
		}
		
        int numKeys = 0;
		const bool* keys = SDL_GetKeyboardState(&numKeys);

		// 今押されているか、前回の update の後に押された (すでに離されていても 1 フレームは押されたことにする)
		const auto isPressed = [&](const SDL_Scancode scancode)
		{
			return ((scancode < numKeys) && keys[scancode])
				|| ((scancode < SDL_SCANCODE_COUNT) && m_downSinceUpdate[scancode]);
		};
		
		for (auto [index, scancode] : detail::KeyConversionTable)
		{
            if (scancode != SDL_SCANCODE_UNKNOWN && scancode < numKeys)
            {
			    m_states[index].update(isPressed(scancode));
            }
		}
		
		{
			const bool shiftPressed = isPressed(SDL_SCANCODE_LSHIFT) || isPressed(SDL_SCANCODE_RSHIFT);
			m_states[0x10].update(shiftPressed);
		}
		
		{
			const bool controlPressed = isPressed(SDL_SCANCODE_LCTRL) || isPressed(SDL_SCANCODE_RCTRL);
			m_states[0x11].update(controlPressed);
		}
		
		{
			const bool altPressed = isPressed(SDL_SCANCODE_LALT) || isPressed(SDL_SCANCODE_RALT);
			m_states[0x12].update(altPressed);
		}

		m_downSinceUpdate.fill(false);

		{
			m_allInputs.clear();

			for (uint32 i = 8; i < 0xEF; ++i)
			{
				const auto& state = m_states[i];

				if (state.pressed() || state.up())
				{
					m_allInputs.emplace_back(InputDeviceType::Keyboard, static_cast<uint8>(i));
				}
			}
		}

		{
			if (m_states[0x1B].down())
			{
				SIV3D_ENGINE(UserAction)->reportUserActions(UserAction::AnyKeyDown | UserAction::EscapeKeyDown);
			}
			else
			{
				for (const auto& input : m_allInputs)
				{
					if (input.down())
					{
						SIV3D_ENGINE(UserAction)->reportUserActions(UserAction::AnyKeyDown);
						break;
					}
				}
			}
		}
	}
    
    bool CKeyboard::down(uint32 index) const
	{
		if (index >= InputState::KeyCount)
		{
			return false;
		}

		return m_states[index].down();
	}

	bool CKeyboard::pressed(uint32 index) const
	{
		if (index >= InputState::KeyCount)
		{
			return false;
		}

		return m_states[index].pressed();
	}

	bool CKeyboard::up(uint32 index) const
	{
		if (index >= InputState::KeyCount)
		{
			return false;
		}

		return m_states[index].up();
	}

	void CKeyboard::clearInput(uint32 index)
	{
		if (index >= InputState::KeyCount)
		{
			return;
		}

		m_states[index].clearInput();
	}

	bool CKeyboard::cleared(uint32 index) const
	{
		if (index >= InputState::KeyCount)
		{
			return false;
		}

		return m_states[index].cleared();
	}

	Duration CKeyboard::pressedDuration(const uint32 index) const
	{
		assert(index < InputState::KeyCount);

		return m_states[index].pressedDuration;
	}

	const String& CKeyboard::name(const uint32 index) const
	{
		assert(index < InputState::KeyCount);

		return m_names[index];
	}

	const Array<Input>& CKeyboard::getAllInput() const noexcept
	{
		return m_allInputs;
	}

	Array<KeyEvent> CKeyboard::getEvents() const noexcept
	{
		return {};
	}
}
