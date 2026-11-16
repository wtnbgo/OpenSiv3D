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
		
		void onTextInput(const char* text);

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

		// 前回の update の後に、テキスト入力の結果 (getChars / getEditingText) が読まれたか。
		// Android / iOS では、テキストボックスなどが入力を受け付けている間 (毎フレーム読まれる) だけ
		// テキスト入力 (ソフトウェアキーボード) を有効にする
		mutable bool m_inputRequested = false;

		int32 m_idleFrames = 0;

		int32 m_requestedFrames = 0;

		void setTextInputActive(bool active);
	};
}
