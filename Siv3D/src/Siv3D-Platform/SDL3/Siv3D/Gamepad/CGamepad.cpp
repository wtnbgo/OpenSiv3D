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
# include <Siv3D/Unicode.hpp>
# include <Siv3D/InputDevice.hpp>
# include <Siv3D/Common/Siv3DEngine.hpp>
# include <Siv3D/Common/OpenGL.hpp>
# include "CGamepad.hpp"

namespace s3d
{
	CGamepad::CGamepad()
		: m_states()
		, m_inputs{ {detail::Gamepad_impl(0), detail::Gamepad_impl(1),
					detail::Gamepad_impl(2), detail::Gamepad_impl(3),
					detail::Gamepad_impl(4), detail::Gamepad_impl(5),
					detail::Gamepad_impl(6), detail::Gamepad_impl(7),
					detail::Gamepad_impl(8), detail::Gamepad_impl(9),
					detail::Gamepad_impl(10), detail::Gamepad_impl(11),
					detail::Gamepad_impl(12), detail::Gamepad_impl(13),
					detail::Gamepad_impl(14), detail::Gamepad_impl(15) } } {}

	CGamepad::~CGamepad()
	{
		LOG_SCOPED_TRACE(U"CGamepad::~CGamepad()");
        
        for (auto& gamepad : m_gamepadHandles)
        {
            if (gamepad)
            {
                SDL_CloseGamepad(gamepad);
                gamepad = nullptr;
            }
        }
	}

	void CGamepad::init()
	{
		LOG_SCOPED_TRACE(U"CGamepad::init()");
        
        SDL_InitSubSystem(SDL_INIT_GAMEPAD);
		
		update();
	}

	void CGamepad::update()
	{
        int count = 0;
        SDL_JoystickID* joysticks = SDL_GetGamepads(&count);
        
        std::array<bool, Gamepad.MaxPlayerCount> active = {};
        
        if (joysticks)
        {
            for (int i = 0; i < count; ++i)
            {
                SDL_JoystickID id = joysticks[i];
                
                int32 foundIndex = -1;
                for (size_t k = 0; k < Gamepad.MaxPlayerCount; ++k)
                {
                    if (m_gamepadHandles[k] && SDL_GetGamepadID(m_gamepadHandles[k]) == id)
                    {
                        foundIndex = static_cast<int32>(k);
                        break;
                    }
                }
                
                if (foundIndex != -1)
                {
                    active[foundIndex] = true;
                }
                else
                {
                    for (size_t k = 0; k < Gamepad.MaxPlayerCount; ++k)
                    {
                        if (!m_gamepadHandles[k])
                        {
                            m_gamepadHandles[k] = SDL_OpenGamepad(id);
                            if (m_gamepadHandles[k])
                            {
                                active[k] = true;
                                auto& state = m_states[k];
                                state.info.playerIndex = static_cast<uint32>(k);
                                state.info.vendorID = SDL_GetGamepadVendor(m_gamepadHandles[k]);
                                state.info.productID = SDL_GetGamepadProduct(m_gamepadHandles[k]);
                                state.info.name = Unicode::Widen(SDL_GetGamepadName(m_gamepadHandles[k]));
                                state.connected = true;
                                LOG_INFO(U"🎮 Gamepad({}) `{}` connected"_fmt(k, state.info.name));
                            }
                            break;
                        }
                    }
                }
            }
            SDL_free(joysticks);
        }
        
        for (size_t i = 0; i < Gamepad.MaxPlayerCount; ++i)
        {
            auto& state = m_states[i];
            SDL_Gamepad* gamepad = m_gamepadHandles[i];
            
            if (gamepad && active[i])
            {
                // axes
                state.axes.clear();
                state.axes << (SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTX) / 32768.0);
                state.axes << (SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFTY) / 32768.0);
                state.axes << (SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX) / 32768.0);
                state.axes << (SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY) / 32768.0);
                state.axes << (SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) / 32768.0);
                state.axes << (SDL_GetGamepadAxis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) / 32768.0);
                
                // buttons (InputState::update)
                state.buttons.resize(SDL_GAMEPAD_BUTTON_COUNT);
                for (int b = 0; b < SDL_GAMEPAD_BUTTON_COUNT; ++b)
                {
                    const bool currentPressed = SDL_GetGamepadButton(gamepad, static_cast<SDL_GamepadButton>(b));
                    state.buttons[b].update(currentPressed);
                }

                // POV (D-pad)
                {
                    const bool up    = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP);
                    const bool right = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT);
                    const bool down  = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN);
                    const bool left  = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT);

                    state.povs[0].update(up);
                    state.povs[1].update(right);
                    state.povs[2].update(down);
                    state.povs[3].update(left);

                    if ((not up) && (not right) && (not down) && (not left))
                    {
                        state.povDegree = none;
                    }
                    else
                    {
                        state.povDegree = detail::GetPOVDirection(up, right, down, left) * 45;
                    }
                }
            }
            else
            {
                if (gamepad)
                {
                    LOG_INFO(U"🎮 Gamepad({}) `{}` disconnected"_fmt(i, state.info.name));
                    SDL_CloseGamepad(gamepad);
                    m_gamepadHandles[i] = nullptr;
                }
                
                state.reset();
            }
        }

        // Rebuild m_inputs from m_states
        for (uint32 playerIndex = 0; playerIndex < Gamepad.MaxPlayerCount; ++playerIndex)
        {
            const auto& src = m_states[playerIndex];
            auto& dst = m_inputs[playerIndex];

            dst.axes = src.axes;
            dst.buttons.clear();

            for (uint32 b = 0; b < src.buttons.size(); ++b)
            {
                dst.buttons.emplace_back(InputDeviceType::Gamepad, static_cast<uint8>(b), static_cast<uint8>(playerIndex));
            }
        }
	}

	Array<GamepadInfo> CGamepad::enumerate()
	{
		Array<GamepadInfo> results;
		for (const auto& state : m_states)
		{
			if (state.connected)
			{
				results << state.info;
			}
		}
		return results;
	}

	bool CGamepad::isConnected(size_t playerIndex)
	{
		if (playerIndex >= Gamepad.MaxPlayerCount) return false;
		return m_states[playerIndex].connected;
	}

	const GamepadInfo& CGamepad::getInfo(size_t playerIndex)
	{
		static const GamepadInfo nullInfo;
		if (playerIndex >= Gamepad.MaxPlayerCount) return nullInfo;
		return m_states[playerIndex].info;
	}

	bool CGamepad::down(size_t playerIndex, uint32 index)
	{
		if (playerIndex >= Gamepad.MaxPlayerCount) return false;
		if (index < m_states[playerIndex].buttons.size())
		{
			return m_states[playerIndex].buttons[index].down();
		}
		else if (InRange(index, 0x80u, 0x83u))
		{
			return m_states[playerIndex].povs[(index - 0x80u)].down();
		}
		return false;
	}

	bool CGamepad::pressed(size_t playerIndex, uint32 index)
	{
		if (playerIndex >= Gamepad.MaxPlayerCount) return false;
		if (index < m_states[playerIndex].buttons.size())
		{
			return m_states[playerIndex].buttons[index].pressed();
		}
		else if (InRange(index, 0x80u, 0x83u))
		{
			return m_states[playerIndex].povs[(index - 0x80u)].pressed();
		}
		return false;
	}

	bool CGamepad::up(size_t playerIndex, uint32 index)
	{
		if (playerIndex >= Gamepad.MaxPlayerCount) return false;
		if (index < m_states[playerIndex].buttons.size())
		{
			return m_states[playerIndex].buttons[index].up();
		}
		else if (InRange(index, 0x80u, 0x83u))
		{
			return m_states[playerIndex].povs[(index - 0x80u)].up();
		}
		return false;
	}

	void CGamepad::clearInput(size_t playerIndex, uint32 index)
	{
		if (playerIndex >= Gamepad.MaxPlayerCount) return;
		if (index < m_states[playerIndex].buttons.size())
		{
			m_states[playerIndex].buttons[index].clearInput();
		}
		else if (InRange(index, 0x80u, 0x83u))
		{
			m_states[playerIndex].povs[(index - 0x80u)].clearInput();
		}
	}

	bool CGamepad::cleared(size_t playerIndex, uint32 index)
	{
		if (playerIndex >= Gamepad.MaxPlayerCount) return false;
		if (index < m_states[playerIndex].buttons.size())
		{
			return m_states[playerIndex].buttons[index].cleared();
		}
		else if (InRange(index, 0x80u, 0x83u))
		{
			return m_states[playerIndex].povs[(index - 0x80u)].cleared();
		}
		return false;
	}

	Duration CGamepad::pressedDuration(size_t playerIndex, uint32 index)
	{
		if (playerIndex >= Gamepad.MaxPlayerCount) return Duration(0);
		if (index < m_states[playerIndex].buttons.size())
		{
			return m_states[playerIndex].buttons[index].pressedDuration;
		}
		else if (InRange(index, 0x80u, 0x83u))
		{
			return m_states[playerIndex].povs[(index - 0x80u)].pressedDuration;
		}
		return Duration(0);
	}

	Optional<int32> CGamepad::povDegree(size_t playerIndex)
	{
		if (playerIndex >= Gamepad.MaxPlayerCount) return none;
		return m_states[playerIndex].povDegree;
	}

	const detail::Gamepad_impl& CGamepad::getInput(size_t playerIndex)
	{
		static const detail::Gamepad_impl nullInput(0);
		if (playerIndex >= Gamepad.MaxPlayerCount) return nullInput;
		return m_inputs[playerIndex];
	}
}
