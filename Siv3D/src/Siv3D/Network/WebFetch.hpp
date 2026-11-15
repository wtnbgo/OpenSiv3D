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
# include <Siv3D/Common.hpp>

# if SIV3D_PLATFORM(WEB)

# include <cstring>
# include <string>
# include <vector>
# include <emscripten.h>
# include <emscripten/fetch.h>
# include <emscripten/threading.h>
# include <Siv3D/IWriter.hpp>
# include <Siv3D/HashTable.hpp>
# include <Siv3D/String.hpp>
# include <Siv3D/EngineLog.hpp>
# include <Siv3D/FormatLiteral.hpp>

//
// Web 版の HTTP (SimpleHTTP / AsyncHTTPTask) を Emscripten の fetch API で行う (curl の代わり)。
//   - ワーカースレッド (AsyncHTTPTask の処理) からは同期 fetch (EMSCRIPTEN_FETCH_SYNCHRONOUS) を使う
//   - メインスレッド (SimpleHTTP の同期関数) からは非同期 fetch を投げて、完了まで emscripten_sleep で待つ
//     (ASYNCIFY でブラウザに制御を返しながら待つので、ページは固まらない)
// 結果は curl と同じく「ステータス行 + レスポンスヘッダ」の文字列にして HTTPResponse に渡す。
// ブラウザの制約 (CORS など) を受ける点はデスクトップ版と異なる。
//

namespace s3d::detail
{
	struct WebFetchResult
	{
		bool ok = false;

		// "HTTP/1.1 200 OK\r\nheader: value\r\n..." (HTTPResponse のコンストラクタに渡す)
		std::string responseHeaders;
	};

	[[nodiscard]]
	inline WebFetchResult WebFetch(const char* method, const String& url, const HashTable<String, String>& headers,
		const void* body, const size_t bodySize, IWriter& writer)
	{
		emscripten_fetch_attr_t attr;
		emscripten_fetch_attr_init(&attr);
		std::strncpy(attr.requestMethod, method, (sizeof(attr.requestMethod) - 1));

		const bool onMainThread = emscripten_is_main_runtime_thread();
		attr.attributes = (EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | EMSCRIPTEN_FETCH_REPLACE);

		if (not onMainThread)
		{
			attr.attributes |= EMSCRIPTEN_FETCH_SYNCHRONOUS;
		}

		// ヘッダ: { name, value, name, value, ..., nullptr }
		std::vector<std::string> headerStrings;
		std::vector<const char*> headerPointers;
		for (auto&& [key, value] : headers)
		{
			headerStrings.push_back(key.toUTF8());
			headerStrings.push_back(value.toUTF8());
		}
		for (const auto& h : headerStrings)
		{
			headerPointers.push_back(h.c_str());
		}
		headerPointers.push_back(nullptr);
		attr.requestHeaders = headerPointers.data();

		if (body && bodySize)
		{
			attr.requestData = static_cast<const char*>(body);
			attr.requestDataSize = bodySize;
		}

		const std::string urlUTF8 = url.toUTF8();
		emscripten_fetch_t* fetch = emscripten_fetch(&attr, urlUTF8.c_str());

		if (not fetch)
		{
			LOG_FAIL(U"WebFetch: emscripten_fetch() failed");
			return{};
		}

		if (onMainThread)
		{
			// readyState 4 (DONE) まで待つ
			while (fetch->readyState != 4)
			{
				emscripten_sleep(1);
			}
		}

		WebFetchResult result;

		if (fetch->status != 0)
		{
			if (fetch->numBytes)
			{
				writer.write(fetch->data, static_cast<int64>(fetch->numBytes));
			}

			std::string statusText = fetch->statusText;
			result.responseHeaders = "HTTP/1.1 " + std::to_string(fetch->status) + " " + statusText + "\r\n";

			const size_t headersLength = emscripten_fetch_get_response_headers_length(fetch);
			if (headersLength)
			{
				std::string raw(headersLength + 1, '\0');
				emscripten_fetch_get_response_headers(fetch, raw.data(), raw.size());
				raw.resize(std::strlen(raw.c_str()));
				result.responseHeaders += raw;
			}

			result.responseHeaders += "\r\n";
			result.ok = true;
		}
		else
		{
			LOG_FAIL(U"WebFetch: request failed (network error or CORS): {}"_fmt(url));
		}

		emscripten_fetch_close(fetch);
		return result;
	}
}

# endif
