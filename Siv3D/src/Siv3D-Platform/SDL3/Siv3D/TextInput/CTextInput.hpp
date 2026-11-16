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
# include <Siv3D/TextInput/ITextInput.hpp>
# include <Siv3D/Stopwatch.hpp>
# include <Siv3D/Common/OpenGL.hpp>

namespace s3d
{
	class CTextInput final : public ISiv3DTextInput
	{
	public:

		CTextInput();

		~CTextInput() override;

		void init() override;

		void update() override;

		void pushChar(uint32 ch) override;

		const String& getChars() const override;

		const String& getEditingText() const override;

		void enableIME(bool enabled) override;

		std::pair<int32, int32> getCursorIndex() const override;
		
		const Array<String>& getCandidates() const override;

		const Array<UnderlineStyle>& getEditingTextStyle() const override;
		
		void setEditingArea(const RectF& sceneArea) override;

		void onTextInput(const char* text);

		// キーの押下 (繰り返しを含む)。BS / Delete / Enter / Tab を制御文字として文字入力に加える
		void onKeyDown(SDL_Scancode scancode);

		void onTextEditing(const char* text, int32 start, int32 length);

	private:

		std::mutex m_mutexChars;
		
		String m_internalChars;
		
		String m_chars;
		
		
		std::mutex m_mutexPreeditStatus;
		
		Array<UnderlineStyle> m_internalPreeditTextStyle;

		String m_internalPreeditText;
		
		int32 m_internalPreeditCaret = 0;
		
		Array<UnderlineStyle> m_preeditTextStyle;

		String m_preeditText;

		int32 m_preeditCaret = 0;
		

		bool m_imeEnabled = true;

		SDL_Window* m_window = nullptr;

		// SDL のテキスト入力を開始しているか (SDL_StartTextInput はウィンドウの指定が必須)
		bool m_textInputStarted = false;

		// 前回の update の後に、入力の場所 (setEditingArea) が伝えられたか。
		// Android / iOS では、テキストボックスなどが入力中 (毎フレーム伝えてくる) の間だけ
		// テキスト入力 (ソフトウェアキーボード) を有効にする
		bool m_areaRequested = false;

		// テキスト入力を始めてから、キーボードが表示されたか (表示されてから消えたら、ユーザが閉じた)
		bool m_keyboardShown = false;

		// ユーザがキーボードを閉じた。入力欄がもう一度タップされるか、入力をやめるまでは出さない
		bool m_keyboardDismissed = false;

		int32 m_idleFrames = 0;

		int32 m_requestedFrames = 0;

		// SDL に伝えた入力の場所 (ウィンドウの座標)。同じなら呼ばない
		SDL_Rect m_editingArea{ -1, -1, 0, 0 };

		void setTextInputActive(bool active);
	};
}
