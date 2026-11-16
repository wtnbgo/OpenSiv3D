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

# include <Siv3D/Resource.hpp>
# include <Siv3D/FileSystem.hpp>

namespace s3d
{
	namespace detail::init
	{
		const Array<FilePath>& GetResourceFilePaths() noexcept;

		static FilePath g_customResourceDirectory;

		void SetCustomResourceDirectory(const FilePath& path)
		{
			g_customResourceDirectory = path;

			if (g_customResourceDirectory && !g_customResourceDirectory.ends_with(U'/'))
			{
				g_customResourceDirectory += U'/';
			}
		}
	}

	const Array<FilePath>& EnumResourceFiles() noexcept
	{
		return detail::init::GetResourceFilePaths();
	}

	FilePath Resource(const FilePathView path)
	{
		if (detail::init::g_customResourceDirectory)
		{
			return (detail::init::g_customResourceDirectory + path);
		}

	# if SIV3D_PLATFORM(IOS)
		// iOS の .app は平らな構成で、直下の "Resources" (大文字小文字を区別しない) は旧形式のバンドルと
		// 解釈されてインストールできなくなるため、別の名前にする
		const FilePath resourceDirectory = (FileSystem::ParentPath(FileSystem::ModulePath()) + U"siv3d_resources/");
	# else
		const FilePath resourceDirectory = (FileSystem::ParentPath(FileSystem::ModulePath()) + U"resources/");
	# endif

		return (resourceDirectory + path);
	}
}
