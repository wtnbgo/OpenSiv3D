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
# include <Siv3D/Keyboard.hpp>
# include <Siv3D/Unicode.hpp>
# include <Siv3D/Window/IWindow.hpp>
# include <Siv3D/Common/Siv3DEngine.hpp>
# include "CTextInput.hpp"

namespace s3d
{
	CTextInput::CTextInput()
	{
	}

	CTextInput::~CTextInput()
	{
	}

	void CTextInput::init()
	{
		LOG_SCOPED_TRACE(U"CTextInput::init()");

		SDL_StartTextInput(nullptr);
	}

	void CTextInput::update()
	{
		{
			std::lock_guard lock{ m_mutexChars };
			
			m_chars = m_internalChars;
			
			m_internalChars.clear();
		}
		
		{
			std::lock_guard lock{ m_mutexPreeditStatus };
			
			m_preeditText = m_internalPreeditText;
			
			m_preeditTextStyle = m_internalPreeditTextStyle;
			
			m_preeditCaret = m_internalPreeditCaret;
		}
	}

	void CTextInput::pushChar(const uint32 ch)
	{
		std::lock_guard lock{ m_mutexChars };

		m_internalChars += static_cast<char32>(ch);
	}

	const String& CTextInput::getChars() const
	{
		return m_chars;
	}

	const String& CTextInput::getEditingText() const
	{
		return m_preeditText;
	}

	void CTextInput::enableIME(bool enabled)
	{
		if (m_imeEnabled == enabled)
		{
			return;
		}

		m_imeEnabled = enabled;

		if (m_imeEnabled)
		{
			SDL_StartTextInput(nullptr);
		}
		else
		{
			SDL_StopTextInput(nullptr);
		}
	}

	std::pair<int32, int32> CTextInput::getCursorIndex() const
	{
		return{ m_preeditCaret, static_cast<int32>(m_preeditText.length()) };
	}

	const Array<String>& CTextInput::getCandidates() const
	{
		static const Array<String> dummy;
		return dummy;
	}

	const Array<UnderlineStyle>& CTextInput::getEditingTextStyle() const
	{
		return m_preeditTextStyle;
	}

	void CTextInput::onTextInput(const char* text)
	{
		const String s = Unicode::FromUTF8(text);
		
		std::lock_guard lock{ m_mutexChars };
		
		m_internalChars += s;
	}

	void CTextInput::onTextEditing(const char* text, int32 start, int32 length)
	{
		std::lock_guard lock{ m_mutexPreeditStatus };
		
		m_internalPreeditText = Unicode::FromUTF8(text);
		
		m_internalPreeditCaret = (start + length);
		
		m_internalPreeditTextStyle.clear();
		
		if (m_internalPreeditText)
		{
			m_internalPreeditTextStyle.resize(m_internalPreeditText.length(), UnderlineStyle::Underline);
		}
	}
}
