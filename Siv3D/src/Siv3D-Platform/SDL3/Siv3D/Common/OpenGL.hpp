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
# include <SDL3/SDL.h>
# include <glad/gles2.h>

namespace s3d
{
	void CheckOpenGLError();

	/// @brief 画面 (ウィンドウ) に描画するときのフレームバッファを返します。
	/// @remark 通常は 0 (既定のフレームバッファ)。iOS では SDL の UIKit バックエンドが作ったフレームバッファ
	[[nodiscard]]
	GLuint GetDefaultFramebuffer();
}
