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

# include <Siv3D/MessageBox.hpp>
# include <Siv3D/Common/OpenGL.hpp>

namespace s3d
{
	namespace detail
	{
		enum class MessageBoxButtons
		{
			OK,

			OKCancel,

			YesNo,
		};

		static MessageBoxResult ShowMessageBox(const StringView title, const StringView text, MessageBoxStyle style, MessageBoxButtons buttons)
		{
			Uint32 flags = 0;
			switch (style)
			{
			case MessageBoxStyle::Info: flags |= SDL_MESSAGEBOX_INFORMATION; break;
			case MessageBoxStyle::Warning: flags |= SDL_MESSAGEBOX_WARNING; break;
			case MessageBoxStyle::Error: flags |= SDL_MESSAGEBOX_ERROR; break;
			default: break;
			}

			SDL_MessageBoxButtonData buttonData[3];
			int buttonCount = 0;

			// SDL_MessageBoxButtonData: flags, buttonid, text
			// Return key default: SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT
			// Escape key default: SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT

			switch (buttons)
			{
			case MessageBoxButtons::OK:
				buttonData[0] = { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT | SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 0, "OK" };
				buttonCount = 1;
				break;
			case MessageBoxButtons::OKCancel:
				buttonData[0] = { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "OK" };
				buttonData[1] = { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 1, "Cancel" };
				buttonCount = 2;
				break;
			case MessageBoxButtons::YesNo:
				buttonData[0] = { SDL_MESSAGEBOX_BUTTON_RETURNKEY_DEFAULT, 0, "Yes" };
				buttonData[1] = { SDL_MESSAGEBOX_BUTTON_ESCAPEKEY_DEFAULT, 1, "No" };
				buttonCount = 2;
				break;
			}

			const SDL_MessageBoxData messageboxdata = {
				flags,
				nullptr, // window
				title.narrow().c_str(),
				text.narrow().c_str(),
				buttonCount,
				buttonData,
				nullptr // colorScheme
			};

			int buttonid;
			if (SDL_ShowMessageBox(&messageboxdata, &buttonid) < 0)
			{
				return MessageBoxResult::Cancel;
			}

			if (buttons == MessageBoxButtons::OK)
			{
				return MessageBoxResult::OK;
			}
			else if (buttons == MessageBoxButtons::OKCancel)
			{
				return (buttonid == 0) ? MessageBoxResult::OK : MessageBoxResult::Cancel;
			}
			else if (buttons == MessageBoxButtons::YesNo)
			{
				return (buttonid == 0) ? MessageBoxResult::Yes : MessageBoxResult::No;
			}

			return MessageBoxResult::OK;
		}
	}

	namespace System
	{
		MessageBoxResult MessageBoxOK(const StringView title, const StringView text, const MessageBoxStyle style)
		{
			return detail::ShowMessageBox(title, text, style, detail::MessageBoxButtons::OK);
		}

		MessageBoxResult MessageBoxOKCancel(const StringView title, const StringView text, const MessageBoxStyle style)
		{
			return detail::ShowMessageBox(title, text, style, detail::MessageBoxButtons::OKCancel);
		}

		MessageBoxResult MessageBoxYesNo(const StringView title, const StringView text, const MessageBoxStyle style)
		{
			return detail::ShowMessageBox(title, text, style, detail::MessageBoxButtons::YesNo);
		}
	}
}
