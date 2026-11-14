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

# import <Foundation/Foundation.h>

namespace s3d
{
	// ファイル / ディレクトリを Finder のゴミ箱へ移す (「戻す」で元の場所に戻せる)
	bool MacOS_TrashFile(const char* path)
	{
		@autoreleasepool
		{
			NSString* nsPath = [NSString stringWithUTF8String:path];

			if (nsPath == nil)
			{
				return false;
			}

			NSURL* url = [NSURL fileURLWithPath:nsPath];
			NSError* error = nil;

			return [[NSFileManager defaultManager] trashItemAtURL:url resultingItemURL:nil error:&error];
		}
	}
}
