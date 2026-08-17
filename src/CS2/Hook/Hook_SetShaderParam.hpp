#pragma once

#include <cstdint>
#include <emmintrin.h>

auto Hook_SetShaderParam( __m128i* map, std::uint32_t hash, __m128i* value ) -> std::uintptr_t;

using SetShaderParam_t = decltype( &Hook_SetShaderParam );
inline SetShaderParam_t SetShaderParam_o = nullptr;
