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

# import <Cocoa/Cocoa.h>

# if !__has_feature(objc_arc)
#	error This file must be compiled with ARC (-fobjc-arc)
# endif

//
// macOS 固有のファイルダイアログ補助 (SivDialog_SDL3.cpp から呼ぶ)
//
// - Edit メニュー:
//   SDL が作るアプリメニューには Edit メニューが無いため、NSOpenPanel / NSSavePanel 内で
//   ⌘A / ⌘C / ⌘V / ⌘X / ⌘Z が効かない (これらのキーはメニュー経由でファーストレスポンダに届く)。
//   ダイアログ表示中だけ標準の Edit メニューを追加する。
//   常設にしないのは、ゲームウィンドウ側の ⌘C / ⌘V 等がメニューに取られて
//   (SDL の IME 用 NSTextView に paste: 等が送られて) Siv3D のキー入力に届かなくなるのを避けるため。
//
// - 終了要求でのキャンセル:
//   SDL3 には表示中のファイルダイアログを外から閉じる API が無いので、
//   表示中のパネルに cancel: を送る。SDL の完了ハンドラがキャンセル (空の結果) で呼ばれる。
//

namespace s3d
{
	namespace
	{
		// ダイアログ表示中だけ追加した Edit メニュー (nil なら追加していない)
		NSMenuItem* g_editMenuItem = nil;

		[[nodiscard]]
		bool HasEditMenu(NSMenu* mainMenu)
		{
			for (NSMenuItem* item in [mainMenu itemArray])
			{
				NSMenu* submenu = [item submenu];

				if (submenu && ([submenu indexOfItemWithTarget:nil andAction:@selector(paste:)] >= 0))
				{
					return true;
				}
			}

			return false;
		}

		[[nodiscard]]
		NSMenuItem* CreateEditMenuItem()
		{
			NSMenu* editMenu = [[NSMenu alloc] initWithTitle:@"Edit"];

			[editMenu addItemWithTitle:@"Undo" action:@selector(undo:) keyEquivalent:@"z"];

			NSMenuItem* redo = [editMenu addItemWithTitle:@"Redo" action:@selector(redo:) keyEquivalent:@"z"];
			[redo setKeyEquivalentModifierMask:(NSEventModifierFlagCommand | NSEventModifierFlagShift)];

			[editMenu addItem:[NSMenuItem separatorItem]];
			[editMenu addItemWithTitle:@"Cut" action:@selector(cut:) keyEquivalent:@"x"];
			[editMenu addItemWithTitle:@"Copy" action:@selector(copy:) keyEquivalent:@"c"];
			[editMenu addItemWithTitle:@"Paste" action:@selector(paste:) keyEquivalent:@"v"];
			[editMenu addItemWithTitle:@"Select All" action:@selector(selectAll:) keyEquivalent:@"a"];

			NSMenuItem* editMenuItem = [[NSMenuItem alloc] initWithTitle:@"Edit" action:nil keyEquivalent:@""];
			[editMenuItem setSubmenu:editMenu];

			return editMenuItem;
		}
	}

	void MacOS_BeginFileDialog()
	{
		@autoreleasepool
		{
			NSMenu* mainMenu = [NSApp mainMenu];

			if ((mainMenu == nil) || g_editMenuItem || HasEditMenu(mainMenu))
			{
				return;
			}

			g_editMenuItem = CreateEditMenuItem();

			// アプリメニューの直後 (標準の位置) に挿入する
			const NSInteger index = (([mainMenu numberOfItems] > 0) ? 1 : 0);
			[mainMenu insertItem:g_editMenuItem atIndex:index];
		}
	}

	void MacOS_EndFileDialog()
	{
		@autoreleasepool
		{
			if (g_editMenuItem == nil)
			{
				return;
			}

			NSMenu* mainMenu = [NSApp mainMenu];

			if (mainMenu && ([mainMenu indexOfItem:g_editMenuItem] >= 0))
			{
				[mainMenu removeItem:g_editMenuItem];
			}

			g_editMenuItem = nil;
		}
	}

	void MacOS_CancelFileDialogs()
	{
		@autoreleasepool
		{
			for (NSWindow* window in [NSApp windows])
			{
				// NSOpenPanel は NSSavePanel のサブクラス
				if ([window isKindOfClass:[NSSavePanel class]] && [window isVisible])
				{
					// シート表示の場合も cancel: で完了ハンドラが NSModalResponseCancel で呼ばれる
					[(NSSavePanel*)window cancel:nil];
				}
			}
		}
	}
}
