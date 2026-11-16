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

# include <cfloat>
# include <Siv3D/ShaderStage.hpp>
# include <string_view>
# include <Siv3D/EngineLog.hpp>
# include <Siv3D/Utility.hpp>
# include "GLES3SamplerState.hpp"

namespace s3d
{
	namespace detail
	{
		static constexpr GLint minmipTable[4] =
		{
			GL_NEAREST_MIPMAP_NEAREST,
			GL_NEAREST_MIPMAP_LINEAR,
			GL_LINEAR_MIPMAP_NEAREST,
			GL_LINEAR_MIPMAP_LINEAR
		};

		// サンプラーの任意機能のうち、この GL コンテキストで使えるもの (最初のサンプラー作成時に一度だけ調べる)。
		// GLES 3.0 / WebGL 2 では、異方性フィルタは EXT_texture_filter_anisotropic、境界色 (GL_TEXTURE_BORDER_COLOR /
		// GL_CLAMP_TO_BORDER) は GLES 3.2 か OES / EXT_texture_border_clamp が無いと使えず、設定すると GL_INVALID_ENUM になる
		struct SamplerCaps
		{
			bool anisotropy = false;

			float maxAnisotropy = 1.0f;

			bool borderClamp = false;
		};

		[[nodiscard]]
		static const SamplerCaps& GetSamplerCaps()
		{
			static const SamplerCaps caps = []()
			{
				SamplerCaps result;

				GLint major = 0, minor = 0;
				::glGetIntegerv(GL_MAJOR_VERSION, &major);
				::glGetIntegerv(GL_MINOR_VERSION, &minor);
				result.borderClamp = ((3 < major) || ((major == 3) && (2 <= minor)));

				GLint extensionCount = 0;
				::glGetIntegerv(GL_NUM_EXTENSIONS, &extensionCount);

				for (GLint i = 0; i < extensionCount; ++i)
				{
					const char* name = reinterpret_cast<const char*>(::glGetStringi(GL_EXTENSIONS, i));

					if (not name)
					{
						continue;
					}

					const std::string_view extension{ name };

					if (extension == "GL_EXT_texture_filter_anisotropic")
					{
						result.anisotropy = true;
					}
					else if ((extension == "GL_OES_texture_border_clamp") || (extension == "GL_EXT_texture_border_clamp"))
					{
						result.borderClamp = true;
					}
				}

				if (result.anisotropy)
				{
					::glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &result.maxAnisotropy);
				}

				LOG_INFO(U"GLES3SamplerState: anisotropy = {} (max {}), border clamp = {}"_fmt(result.anisotropy, result.maxAnisotropy, result.borderClamp));
				return result;
			}();

			return caps;
		}
	}

	static const SamplerState NullSamplerState(TextureAddressMode::Repeat,
		TextureAddressMode::Repeat,
		TextureAddressMode::Repeat,
		TextureFilter::Linear,
		TextureFilter::Linear,
		TextureFilter::Linear,
		0,
		-1,
		Float4(FLT_MIN, FLT_MIN, FLT_MIN, FLT_MIN));

	GLES3SamplerState::GLES3SamplerState()
	{
		m_currentVSStates.fill(NullSamplerState);
		m_currentPSStates.fill(NullSamplerState);
	}

	void GLES3SamplerState::setVS(const uint32 slot, const SamplerState& state)
	{
		assert(slot < SamplerState::MaxSamplerCount);

		if (state == m_currentVSStates[slot])
		{
			return;
		}

		auto it = m_states.find(state);

		if (it == m_states.end())
		{
			it = create(state);

			if (it == m_states.end())
			{
				return;
			}
		}

		::glBindSampler(Shader::Internal::MakeSamplerSlot(ShaderStage::Vertex, slot), it->second->m_sampler);

		m_currentVSStates[slot] = state;
	}

	void GLES3SamplerState::setVS(const uint32 slot, None_t)
	{
		assert(slot < SamplerState::MaxSamplerCount);

		::glBindSampler(Shader::Internal::MakeSamplerSlot(ShaderStage::Vertex, slot), 0);

		m_currentVSStates[slot] = NullSamplerState;
	}

	void GLES3SamplerState::setPS(const uint32 slot, const SamplerState& state)
	{
		assert(slot < SamplerState::MaxSamplerCount);

		if (state == m_currentPSStates[slot])
		{
			return;
		}

		auto it = m_states.find(state);

		if (it == m_states.end())
		{
			it = create(state);

			if (it == m_states.end())
			{
				return;
			}
		}

		::glBindSampler(Shader::Internal::MakeSamplerSlot(ShaderStage::Pixel, slot), it->second->m_sampler);

		m_currentPSStates[slot] = state;
	}

	void GLES3SamplerState::setPS(const uint32 slot, None_t)
	{
		assert(slot < SamplerState::MaxSamplerCount);

		::glBindSampler(Shader::Internal::MakeSamplerSlot(ShaderStage::Pixel, slot), 0);

		m_currentPSStates[slot] = NullSamplerState;
	}

	GLES3SamplerState::SamplerStateList::iterator GLES3SamplerState::create(const SamplerState& state)
	{
		std::unique_ptr<SamplerState_GL> samplerState = std::make_unique<SamplerState_GL>();

		const GLuint sampler = samplerState->m_sampler;
		const detail::SamplerCaps& caps = detail::GetSamplerCaps();
		// (以前は static で、最初に作ったサンプラーの境界色が以降のすべてに使われていた)
		const GLfloat border[] = { state.borderColor.x, state.borderColor.y, state.borderColor.z, state.borderColor.w };
		// 境界色が使えない環境では、TextureAddressMode::BorderColor を端の色の繰り返し (CLAMP_TO_EDGE) で代用する
		const GLuint wraps[] = { GL_REPEAT, GL_MIRRORED_REPEAT, GL_CLAMP_TO_EDGE, static_cast<GLuint>(caps.borderClamp ? GL_CLAMP_TO_BORDER : GL_CLAMP_TO_EDGE) };

		::glSamplerParameteri(sampler, GL_TEXTURE_MIN_FILTER,
			detail::minmipTable[(static_cast<int32>(state.min) << 1) | (static_cast<int32>(state.mip))]);
		::glSamplerParameteri(sampler, GL_TEXTURE_MAG_FILTER, (state.mag == TextureFilter::Linear) ? GL_LINEAR : GL_NEAREST);
		::glSamplerParameteri(sampler, GL_TEXTURE_WRAP_S, wraps[static_cast<int32>(state.addressU)]);
		::glSamplerParameteri(sampler, GL_TEXTURE_WRAP_T, wraps[static_cast<int32>(state.addressV)]);
		::glSamplerParameteri(sampler, GL_TEXTURE_WRAP_R, wraps[static_cast<int32>(state.addressW)]);
		// GL_TEXTURE_LOD_BIAS is not available in GLES
		::glSamplerParameteri(sampler, GL_TEXTURE_COMPARE_MODE, GL_NONE);
		if (caps.anisotropy)
		{
			// 1 未満は GL_INVALID_VALUE になる
			::glSamplerParameterf(sampler, GL_TEXTURE_MAX_ANISOTROPY_EXT, Clamp(static_cast<float>(state.maxAnisotropy), 1.0f, caps.maxAnisotropy));
		}
		if (caps.borderClamp)
		{
			::glSamplerParameterfv(sampler, GL_TEXTURE_BORDER_COLOR, border);
		}
		::glSamplerParameterf(sampler, GL_TEXTURE_MIN_LOD, -1000.0f);
		::glSamplerParameterf(sampler, GL_TEXTURE_MAX_LOD, 1000.0f);

		if (m_states.size() >= 1024)
		{
			m_states.clear();
		}

		return m_states.emplace(state, std::move(samplerState)).first;
	}
}
