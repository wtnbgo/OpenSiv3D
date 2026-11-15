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

//
// Web (Emscripten) 専用の公開 API (Browser.hpp / TextInput.hpp の Platform::Web) の SDL3 版の実装。
// 以前の Web 版 (Siv3D-Platform/Web) は GLFW と独自の JS ライブラリに依存していたので、
// EM_JS で必要な分だけ書き直している。
//

# include <emscripten.h>
# include <Siv3D/Browser.hpp>
# include <Siv3D/TextInput.hpp>
# include <Siv3D/FileSystem.hpp>
# include <Siv3D/SimpleHTTP.hpp>
# include <Siv3D/Unicode.hpp>
# include <Siv3D/EngineLog.hpp>

namespace s3d
{
	namespace
	{
		// 仮想ファイルシステム上のファイルを Blob にして、ブラウザのダウンロードとして保存させる
		EM_JS(void, siv3d_download_file, (const char* filePath, const char* fileName), {
			const path = UTF8ToString(filePath);
			const name = UTF8ToString(fileName);
			const data = FS.readFile(path);
			const blob = new Blob([data], { type: "application/octet-stream" });
			const url = URL.createObjectURL(blob);
			const a = document.createElement("a");
			a.href = url;
			a.download = name;
			document.body.appendChild(a);
			a.click();
			document.body.removeChild(a);
			setTimeout(() => URL.revokeObjectURL(url), 1000);
		});

		// location.search (先頭の ? を除く) を返す。呼び出し側で free する
		EM_JS(char*, siv3d_get_url_search, (), {
			// EM_JS の本体は C の文字列になるので、バックスラッシュを使う正規表現は避ける
			const q = (typeof location !== "undefined") ? location.search : "";
			const s = q.startsWith("?") ? q.substring(1) : q;
			return stringToNewUTF8(s);
		});
	}

	namespace Platform::Web
	{
		void DownloadFile(const FilePathView filePath)
		{
			const String fileName = FileSystem::FileName(filePath);
			siv3d_download_file(filePath.toUTF8().c_str(), fileName.toUTF8().c_str());
		}

		void FetchFile(const FilePathView filePath)
		{
			// 仮想ファイルシステムに無ければ、ページと同じ場所から HTTP で取ってくる
			if (not FileSystem::Exists(filePath))
			{
				s3d::SimpleHTTP::Save(filePath, filePath);
			}
		}
	}

	namespace Platform::Web::System
	{
		HashTable<String, String> GetURLParameters()
		{
			HashTable<String, String> result;

			char* search = siv3d_get_url_search();
			const String query = Unicode::FromUTF8(search ? search : "");
			std::free(search);

			for (const auto& pair : query.split(U'&'))
			{
				if (pair.isEmpty())
				{
					continue;
				}

				const size_t pos = pair.indexOf(U'=');

				if (pos == String::npos)
				{
					result.emplace(pair, U"");
				}
				else
				{
					result.emplace(pair.substr(0, pos), pair.substr(pos + 1));
				}
			}

			return result;
		}

		void DisbaleBrowserKeyboardShortcuts(const Array<BrowserKeyboardShorcut>&)
		{
			// SDL3 がキャンバスのキー入力を受け取って既定の動作を止めるので、ここでは何もしない
		}
	}

	namespace Platform::Web::TextInput
	{
		// SDL3 版はテキスト入力・IME を SDL の仕組みで扱うので、以前の Web 版の
		// 隠し <input> 要素との同期 API は何もしない
		std::pair<int32, int32> GetCandicateCursorIndex()
		{
			return{ 0, 0 };
		}

		void SetFocusToTextInput(bool) {}

		void SyncronizeText(StringView) {}

		int32 GetCursorIndex()
		{
			return 0;
		}

		void SetCursorIndex(int32) {}
	}
}
