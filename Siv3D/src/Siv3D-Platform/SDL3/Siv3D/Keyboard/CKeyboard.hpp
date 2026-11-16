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
# include <Siv3D/Keyboard/IKeyboard.hpp>
# include <Siv3D/Input/InputState.hpp>
# include <Siv3D/Common/OpenGL.hpp>

namespace s3d
{
	class CKeyboard final : public ISiv3DKeyboard
	{
	private:

		SDL_Window* m_window = nullptr;

		std::array<String, InputState::KeyCount> m_names;

		std::array<InputState, InputState::KeyCount> m_states;

		Array<Input> m_allInputs;

		// 前回の update の後に押された (SDL_EVENT_KEY_DOWN が来た) キー。SDL_GetKeyboardState はその瞬間の状態なので、
		// 1 フレームの間に押して離したキーは見えない。これを合わせて、少なくとも 1 フレームは押されたものとして扱う
		std::array<bool, SDL_SCANCODE_COUNT> m_downSinceUpdate{};

	public:

		/// @brief キーが押されたイベント (CWindow のイベントループから呼ぶ)
		void onKeyDown(SDL_Scancode scancode) noexcept;

		CKeyboard();

		~CKeyboard() override;

		void init() override;

		void update() override;

		bool down(uint32 index) const override;

		bool pressed(uint32 index) const override;

		bool up(uint32 index) const override;

		void clearInput(uint32 index) override;

		bool cleared(uint32 index) const override;

		Duration pressedDuration(uint32 index) const override;

		const String& name(uint32 index) const override;

		const Array<Input>& getAllInput() const noexcept override;

		Array<KeyEvent> getEvents() const noexcept override;
	};
}
