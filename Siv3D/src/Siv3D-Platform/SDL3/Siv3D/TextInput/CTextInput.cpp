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

# include <utility>
# include <Siv3D/EngineLog.hpp>
# include <Siv3D/FormatLiteral.hpp>
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

	// Android / iOS: ソフトウェアキーボードが出るので、入力を受け付けている間だけテキスト入力を有効にする
	static constexpr bool TextInputOnDemand =
	# if defined(__ANDROID__) || SIV3D_PLATFORM(IOS)
		true;
	# else
		false;
	# endif

	void CTextInput::setTextInputActive(const bool active)
	{
		if ((not m_window) || (m_textInputStarted == active))
		{
			return;
		}

		if (active)
		{
			if (SDL_StartTextInput(m_window))
			{
				m_textInputStarted = true;
			}
			else
			{
				LOG_FAIL(U"SDL_StartTextInput() failed: {}"_fmt(Unicode::FromUTF8(SDL_GetError())));
			}
		}
		else
		{
			SDL_StopTextInput(m_window);
			m_textInputStarted = false;
		}
	}

	void CTextInput::init()
	{
		LOG_SCOPED_TRACE(U"CTextInput::init()");

		// 以前は SDL_StartTextInput(nullptr) で、ウィンドウの指定が無いため SDL3 では失敗し、テキスト入力
		// (SDL_EVENT_TEXT_INPUT / TEXT_EDITING) が届いていなかった。ウィンドウを指定して開始する
		m_window = static_cast<SDL_Window*>(SIV3D_ENGINE(Window)->getHandle());

		if constexpr (not TextInputOnDemand)
		{
			// デスクトップ・Web: 元の Siv3D と同じく常に有効 (IME を使えるようにする)
			setTextInputActive(m_imeEnabled);
		}
	}

	void CTextInput::update()
	{
		if constexpr (TextInputOnDemand)
		{
			// 続けて読まれた (テキストボックスなどが入力中) ならキーボードを出し、数フレーム読まれなければしまう。
			// 1 フレームだけ読まれた場合 (TextBox はクリックのたびに GetEditingText を呼ぶ) には出さない
			if (std::exchange(m_inputRequested, false))
			{
				m_idleFrames = 0;

				if (2 <= ++m_requestedFrames)
				{
					setTextInputActive(m_imeEnabled);
				}
			}
			else
			{
				m_requestedFrames = 0;

				if (m_textInputStarted && (3 < ++m_idleFrames))
				{
					setTextInputActive(false);
				}
			}
		}

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
		m_inputRequested = true;
		return m_chars;
	}

	const String& CTextInput::getEditingText() const
	{
		m_inputRequested = true;
		return m_preeditText;
	}

	void CTextInput::enableIME(bool enabled)
	{
		if (m_imeEnabled == enabled)
		{
			return;
		}

		m_imeEnabled = enabled;

		if constexpr (TextInputOnDemand)
		{
			if (not m_imeEnabled)
			{
				setTextInputActive(false);
			}
		}
		else
		{
			setTextInputActive(m_imeEnabled);
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
