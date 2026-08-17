#pragma once

#include <cstdint>
#include <emmintrin.h>

auto Hook_SetPostProcessVec( __m128i* map, std::uint32_t hash, __m128i* value ) -> std::uintptr_t;

using SetPostProcessVec_t = decltype( &Hook_SetPostProcessVec );
inline SetPostProcessVec_t SetPostProcessVec_o = nullptr;
