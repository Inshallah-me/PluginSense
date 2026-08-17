#pragma once

#include <cstdint>
#include <emmintrin.h>

class CWorldVisuals final
{
public:
	void on_create_move( class CCSGOInput* pInput );
	void on_set_shader_param( __m128i*& value, std::uint32_t hash );

private:
	float m_focus_depth = 1000.0f;
};

auto GetWorldVisuals() -> CWorldVisuals*;
