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
# include <mutex>
# include <Siv3D/Mouse/IMouse.hpp>
# include <Siv3D/Input/InputState.hpp>
# include <Siv3D/PointVector.hpp>
# include <Siv3D/Common/OpenGL.hpp>

namespace s3d
{
	class CMouse final : public ISiv3DMouse
	{
	private:

		enum class MouseButtonState
		{
			Released,
			
			Pressed,
			
			Tapped,
		};

		SDL_Window* m_window = nullptr;

		//
		// Buttons
		//
		
		std::mutex m_buttonMutex;

		std::array<MouseButtonState, InputState::MouseButtonCount> m_buttonsInternal;

		// 前回の update の後に押されたボタン (押して離すのが 1 フレーム以内に終わったら Tapped にする)
		std::array<bool, InputState::MouseButtonCount> m_downSinceUpdate{};

		std::array<InputState, InputState::MouseButtonCount> m_states;

		Array<Input> m_allInputs;

	public:
//		static void onMouseButtonUpdated(int button, bool pressed);
//		static void onScrollUpdated(double x, double y);

	private:
		//
		// Scroll
		//

		std::mutex m_scrollMutex;

		Vec2 m_scrollInternal{ 0.0, 0.0 };

		Vec2 m_scroll{ 0.0, 0.0 };

	public:

		CMouse();

		~CMouse() override;

		void init() override;

		void update() override;

		bool down(uint32 index) const override;

		bool pressed(uint32 index) const override;

		bool up(uint32 index) const override;

		void clearInput(uint32 index) override;

		bool cleared(uint32 index) const override;

		Duration pressedDuration(uint32 index) const override;

		const Array<Input>& getAllInput() const noexcept override;

		const Vec2& wheel() const noexcept override;

		void onMouseButtonUpdated(int32 index, bool pressed) override;

		void onScroll(double v, double h) override;
	};
}
