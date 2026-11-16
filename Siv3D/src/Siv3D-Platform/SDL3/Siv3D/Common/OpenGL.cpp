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

# include "OpenGL.hpp"
# include <Siv3D/EngineLog.hpp>
# include <Siv3D/Window/IWindow.hpp>
# include <Siv3D/Common/Siv3DEngine.hpp>

namespace s3d
{
	void CheckOpenGLError()
	{
		// GL コンテキスト生成 (glad のロード) 前に初期化が失敗した場合、終了処理から呼ばれても関数ポインタが null
		if (glGetError == nullptr)
		{
			return;
		}

		size_t limitter = 0;

		GLenum err;

		while ((err = glGetError()) != GL_NO_ERROR)
		{
			LOG_ERROR(U"OpenGL Error: 0x{:x}"_fmt(err));

			if (++limitter > 30)
			{
				LOG_ERROR(U"OpenGL error report interrupted.");
				break;
			}
		}
	}

	GLuint GetDefaultFramebuffer()
	{
	# if SIV3D_PLATFORM(IOS)
		// iOS の EAGL には既定のフレームバッファ (0) が無く、SDL がビューのフレームバッファを作る
		if (SDL_Window* window = static_cast<SDL_Window*>(SIV3D_ENGINE(Window)->getHandle()))
		{
			return static_cast<GLuint>(SDL_GetNumberProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_UIKIT_OPENGL_FRAMEBUFFER_NUMBER, 0));
		}
		return 0;
	# else
		return 0;
	# endif
	}
}
