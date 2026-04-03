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
	};
}
