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

		const FilePath resourceDirectory = (FileSystem::ParentPath(FileSystem::ModulePath()) + U"resources/");

		return (resourceDirectory + path);
	}
}
