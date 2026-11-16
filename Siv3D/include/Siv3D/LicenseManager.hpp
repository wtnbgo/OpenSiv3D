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
# include "Common.hpp"
# include "LicenseInfo.hpp"
# include "Array.hpp"

namespace s3d
{
	namespace LicenseManager
	{
		/// @brief ライセンス情報一覧の先頭にアプリケーションのライセンスを追加します。
		/// @param applicationName アプリケーションの名前
		/// @param license ライセンス情報
		void SetApplicationLicense(const String& applicationName, const LicenseInfo& license);

		/// @brief ライセンス情報の一覧に新しい要素を追加します。
		/// @param license ライセンス情報
		void AddLicense(const LicenseInfo& license);

		/// @brief フォルダ内のライセンスファイル (*.txt) を読み込んで、ライセンス情報の一覧に追加します。
		/// @param directory ライセンスファイルを置いたフォルダ (例: U"assets/licenses")
		/// @return 追加したライセンスの数
		/// @remark ファイル名の順に追加します。ファイルは UTF-8 のテキストで、先頭の `Title: 名前` と `Copyright: 著作権表示`
		/// (複数行可) の行の後、空行に続けて許諾条件の本文を書きます。`Title:` が無い場合はファイル名を名前にします。
		/// アプリが同梱するフォントや素材のライセンスを、アセットのフォルダに置いたファイルで追加するために使います。
		size_t AddLicenseFiles(FilePathView directory);

		/// @brief ライセンス情報一覧を返します。
		/// @return ライセンス情報一覧
		[[nodiscard]]
		const Array<LicenseInfo>& EnumLicenses();

		/// @brief [F1] キーでライセンス情報を表示する挙動を有効にします（デフォルトでは有効）。
		void EnableDefaultTrigger();

		/// @brief [F1] キーでライセンス情報を表示する挙動を無効化します。
		void DisableDefaultTrigger();

		/// @brief ライセンス情報を Web ブラウザで表示します。
		void ShowInBrowser();
	}
}
