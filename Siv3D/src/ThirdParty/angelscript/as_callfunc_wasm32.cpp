//
// [Siv3D] AngelScript のネイティブ呼び出し規約の WebAssembly (wasm32 / Emscripten) 実装
//
// wasm はスタックやレジスタを直接操作できないので、引数を型付きで並べ、JS から wasm の関数テーブル
// 経由で呼び出す (siv3d_as_call_indirect)。引数と戻り値の渡し方は clang の wasm32 C/C++ ABI に従う。
//   - int32 以下 / ポインタ / 参照 / ハンドル / enum: i32、int64: i64 (BigInt)、float: f32、double: f64
//   - 値渡しの構造体: スカラー 1 つだけの構造体はそのスカラーとして、空の構造体は引数から消え、
//     それ以外はコピーへのポインタ (i32) で渡す
//   - 戻り値の構造体: スカラー 1 つだけならそのスカラー、それ以外は先頭の隠れ引数 (sret) のアドレスに書く
//   - メンバ関数: 引数の並びは sret, this, (OBJFIRST の第 2 オブジェクト), 引数..., (OBJLAST の第 2 オブジェクト)
//   - メンバ関数ポインタは ARM 式 (ptr = 関数 or vtable 内のオフセット、adj = 2 * this の調整 + 仮想なら 1)。
//     仮想かどうかと this の調整は as_callfunc.cpp で AS_ARM64 と同じ扱いにしている
//
// 構造体が「スカラー 1 つだけ」か「空」かは AngelScript の型情報 (フラグ) からは分からないので、
// Siv3D のバインドで値渡しされる型について asWasm32ValueKind() の表で指定する。
//
// 注意: 呼び出しの途中に JS のフレームが入るので、ASYNCIFY でブラウザに制御を返す関数
// (System::Update など) はこの経路で呼べない。そうした関数は asCALL_GENERIC で登録すること。
//

#include "as_config.h"

#ifndef AS_MAX_PORTABILITY
#ifdef AS_WASM32

#include <cstring>
#include <emscripten.h>
#include "as_callfunc.h"
#include "as_scriptengine.h"
#include "as_texts.h"
#include "as_context.h"

// sig: 先頭が戻り値の型、続いて引数の型 ('v' / 'i' / 'j' / 'f' / 'd')、0 終端
// args: 引数を 8 バイトずつ並べたもの、ret: 8 バイトの戻り値の置き場所
EM_JS(void, siv3d_as_call_indirect, (int func, const char* sig, void* ret, const void* args), {
	const f = getWasmTableEntry(func);
	const a = [];
	let p = args;
	for (let i = sig + 1; ; ++i, p += 8) {
		const c = HEAPU8[i];
		if (c === 0) break;
		if (c === 105) a.push(HEAP32[p >> 2]);        // 'i'
		else if (c === 106) a.push(HEAP64[p >> 3]);   // 'j'
		else if (c === 102) a.push(HEAPF32[p >> 2]);  // 'f'
		else a.push(HEAPF64[p >> 3]);                 // 'd'
	}
	const r = f(...a);
	const rt = HEAPU8[sig];
	if (rt === 105) HEAP32[ret >> 2] = r;
	else if (rt === 106) HEAP64[ret >> 3] = r;
	else if (rt === 102) HEAPF32[ret >> 2] = r;
	else if (rt === 100) HEAPF64[ret >> 3] = r;
});
EM_JS_DEPS(siv3d_as_call_indirect_deps, "$getWasmTableEntry");

BEGIN_AS_NAMESPACE

namespace
{
	constexpr int MaxArgs = 64;

	struct ArgList
	{
		alignas(8) asQWORD slots[MaxArgs] = {};
		char sig[MaxArgs + 2] = {};
		int count = 0;

		void pushI32(asDWORD v) { std::memcpy(&slots[count], &v, 4); sig[1 + count++] = 'i'; }
		void pushI64(asQWORD v) { slots[count] = v; sig[1 + count++] = 'j'; }
		void pushF32(const void* p) { std::memcpy(&slots[count], p, 4); sig[1 + count++] = 'f'; }
		void pushF64(const void* p) { std::memcpy(&slots[count], p, 8); sig[1 + count++] = 'd'; }
		void pushPtr(const void* p) { pushI32(static_cast<asDWORD>(reinterpret_cast<asPWORD>(p))); }
	};

	// 1 / 2 バイトの整数は、wasm の ABI どおり 32 ビットに符号拡張 / ゼロ拡張して渡す
	asDWORD ExtendSmallInt(const asCDataType& type, asDWORD v)
	{
		switch (type.GetTokenType())
		{
		case ttInt8:	return static_cast<asDWORD>(static_cast<int>(static_cast<signed char>(v)));
		case ttInt16:	return static_cast<asDWORD>(static_cast<int>(static_cast<short>(v)));
		case ttUInt8:
		case ttBool:	return (v & 0xFFu);
		case ttUInt16:	return (v & 0xFFFFu);
		default:		return v;
		}
	}
}

char asWasm32ValueKind(const asCTypeInfo* typeInfo)
{
	if (!typeInfo)
	{
		return 0;
	}

	const asDWORD flags = typeInfo->flags;

	if (flags & asOBJ_APP_PRIMITIVE)
	{
		return (typeInfo->size > 4) ? 'j' : 'i';
	}

	if (flags & asOBJ_APP_FLOAT)
	{
		return (typeInfo->size > 4) ? 'd' : 'f';
	}

	if (flags & (COMPLEX_MASK | asOBJ_APP_ARRAY))
	{
		return 0;
	}

	// [Siv3D] 値渡しされる型のうち、スカラー 1 つだけの構造体 ('i' / 'j' / 'f' / 'd') と空の構造体 ('e')
	struct Entry { const char* name; char kind; };
	static const Entry entries[] =
	{
		{ "Duration", 'd' },			// std::chrono::duration<double>
		{ "TextureFormat", 'i' },		// TexturePixelFormat 1 つ
		{ "sampleRate_uint32", 'i' },	// Arg::sampleRate_<uint32>
		{ "None_t", 'e' },				// std::nullopt_t
		{ "AudioFileStreaming", 'e' },	// Audio::FileStreaming
		{ "topLeft_", 'e' },			// 名前付き引数のヘルパー (空)
		{ "center_", 'e' },
		{ "sampleRate_", 'e' },
		{ "Print_impl", 'e' },
		{ "Say_impl", 'e' },
		{ "XInput_helper", 'e' },
	};

	for (const auto& entry : entries)
	{
		if (typeInfo->name == entry.name)
		{
			return entry.kind;
		}
	}

	return 0;
}

asQWORD CallSystemFunctionNative(asCContext *context, asCScriptFunction *descr, void *obj, asDWORD *args, void *retPointer, asQWORD &/*retQW2*/, void *secondObject)
{
	asCScriptEngine            *engine  = context->m_engine;
	asSSystemFunctionInterface *sysFunc = descr->sysFuncIntf;
	const int                   callConv = sysFunc->callConv;
	asPWORD                     func    = reinterpret_cast<asPWORD>(sysFunc->func);

	// 仮想関数: func は vtable 内のオフセット (バイト)。wasm の関数ポインタは関数テーブルの添字
	if (obj && (callConv == ICC_VIRTUAL_THISCALL
		|| callConv == ICC_VIRTUAL_THISCALL_OBJFIRST
		|| callConv == ICC_VIRTUAL_THISCALL_OBJLAST))
	{
		const asBYTE* vtable = *reinterpret_cast<asBYTE* const*>(obj);
		func = *reinterpret_cast<const asPWORD*>(vtable + func);
	}

	ArgList list;

	// 戻り値
	const asCDataType& returnType = descr->returnType;
	bool passReturnPointer = false;
	char returnKind = 'v';

	if (sysFunc->hostReturnInMemory)
	{
		// 空の構造体は sret を取らない (戻り値なしと同じ)
		passReturnPointer = (asWasm32ValueKind(returnType.GetTypeInfo()) != 'e');
	}
	else if (returnType.IsObject() && !returnType.IsReference() && !returnType.IsObjectHandle())
	{
		// スカラー 1 つだけの構造体 / APP_PRIMITIVE / APP_FLOAT の値型
		returnKind = asWasm32ValueKind(returnType.GetTypeInfo());
		if (returnKind == 0 || returnKind == 'e')
		{
			returnKind = 'v';
		}
	}
	else if (returnType.IsReference() || returnType.IsObjectHandle())
	{
		returnKind = 'i';
	}
	else if (returnType.IsFloatType())
	{
		returnKind = 'f';
	}
	else if (returnType.IsDoubleType())
	{
		returnKind = 'd';
	}
	else if (returnType.GetSizeInMemoryBytes() == 0)
	{
		returnKind = 'v';
	}
	else
	{
		returnKind = (returnType.GetSizeOnStackDWords() == 2) ? 'j' : 'i';
	}

	list.sig[0] = returnKind;

	if (passReturnPointer)
	{
		list.pushPtr(retPointer);
	}

	// this と第 2 オブジェクト
	bool objLast = false;
	switch (callConv)
	{
	case ICC_THISCALL:
	case ICC_VIRTUAL_THISCALL:
	case ICC_CDECL_OBJFIRST:
		list.pushPtr(obj);
		break;
	case ICC_THISCALL_OBJFIRST:
	case ICC_VIRTUAL_THISCALL_OBJFIRST:
		list.pushPtr(obj);
		list.pushPtr(secondObject);
		break;
	case ICC_THISCALL_OBJLAST:
	case ICC_VIRTUAL_THISCALL_OBJLAST:
		list.pushPtr(obj);
		objLast = true;
		break;
	case ICC_CDECL_OBJLAST:
		objLast = true;
		break;
	default: // ICC_CDECL / ICC_STDCALL
		break;
	}

	// 呼び出しの後で解放する、値渡しの構造体のコピー
	void* toFree[MaxArgs];
	int freeCount = 0;

	const asDWORD* sp = args;
	const asUINT paramCount = descr->parameterTypes.GetLength();
	for (asUINT n = 0; n < paramCount; ++n)
	{
		const asCDataType& type = descr->parameterTypes[n];

		if (type.GetTokenType() == ttQuestion)
		{
			// 可変型の引数: ポインタと型 ID
			list.pushI32(sp[0]);
			list.pushI32(sp[1]);
			sp += 2;
		}
		else if (type.IsReference() || type.IsObjectHandle())
		{
			list.pushI32(*sp);
			sp += AS_PTR_SIZE;
		}
		else if (type.IsObject())
		{
			// 値渡しの構造体 (スタックにはコピーへのポインタがある)
			void* const p = reinterpret_cast<void*>(static_cast<asPWORD>(*sp));
			sp += AS_PTR_SIZE;

			const char kind = asWasm32ValueKind(type.GetTypeInfo());
			const asUINT size = type.GetSizeInMemoryBytes();

			if (kind == 'i')
			{
				asDWORD v = 0;
				std::memcpy(&v, p, (size < 4) ? size : 4);
				list.pushI32(v);
			}
			else if (kind == 'j')
			{
				asQWORD v = 0;
				std::memcpy(&v, p, 8);
				list.pushI64(v);
			}
			else if (kind == 'f')
			{
				list.pushF32(p);
			}
			else if (kind == 'd')
			{
				list.pushF64(p);
			}
			else if (kind == 0)
			{
				// コピーへのポインタをそのまま渡す (呼ばれた側はこのコピーを使う)
				list.pushPtr(p);
			}
			// kind == 'e': 空の構造体は渡さない

			// 複雑な型 (デストラクタ等あり) は as_callfunc.cpp が呼び出し後に破棄・解放する
			if (!(type.GetTypeInfo()->flags & COMPLEX_MASK))
			{
				toFree[freeCount++] = p;
			}
		}
		else if (type.IsFloatType())
		{
			list.pushF32(sp);
			sp += 1;
		}
		else if (type.IsDoubleType())
		{
			list.pushF64(sp);
			sp += 2;
		}
		else if (type.GetSizeOnStackDWords() == 2)
		{
			asQWORD v;
			std::memcpy(&v, sp, 8);
			list.pushI64(v);
			sp += 2;
		}
		else
		{
			list.pushI32(ExtendSmallInt(type, *sp));
			sp += 1;
		}

		if (list.count >= MaxArgs - 2)
		{
			context->SetInternalException(TXT_INVALID_CALLING_CONVENTION);
			return 0;
		}
	}

	if (objLast)
	{
		list.pushPtr((callConv == ICC_CDECL_OBJLAST) ? obj : secondObject);
	}

	list.sig[1 + list.count] = 0;

	alignas(8) asQWORD retQW = 0;
	siv3d_as_call_indirect(static_cast<int>(func), list.sig, &retQW, list.slots);

	for (int i = 0; i < freeCount; ++i)
	{
		engine->CallFree(toFree[i]);
	}

	return retQW;
}

END_AS_NAMESPACE

#endif // AS_WASM32
#endif // AS_MAX_PORTABILITY
