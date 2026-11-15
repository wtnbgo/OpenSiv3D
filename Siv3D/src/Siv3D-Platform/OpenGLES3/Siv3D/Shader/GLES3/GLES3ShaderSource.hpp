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
# include <string>

namespace s3d::detail
{
	/// @brief GLSL ES のソースを、ドライバが受け付けやすい形にそろえる。
	/// @remark 先頭の "# version" ("#" と "version" の間に空白) は仕様上は正しいが、一部の実装
	/// (Android エミュレータの GLES 変換層など) は "unsupported shader version" で拒否するので "#version" にする
	inline void NormalizeGLSLESSource(std::string& source)
	{
		// UTF-8 の BOM
		if (source.starts_with("\xEF\xBB\xBF"))
		{
			source.erase(0, 3);
		}

		const size_t hash = source.find_first_not_of(" \t\r\n");

		if ((hash == std::string::npos) || (source[hash] != '#'))
		{
			return;
		}

		const size_t directive = source.find_first_not_of(" \t", (hash + 1));

		if ((directive != std::string::npos) && (directive != (hash + 1)) && (source.compare(directive, 7, "version") == 0))
		{
			source.erase((hash + 1), (directive - (hash + 1)));
		}
	}
}
