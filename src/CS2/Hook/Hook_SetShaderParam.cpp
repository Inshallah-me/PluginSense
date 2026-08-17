#include "Hook_SetShaderParam.hpp"

#include <PluginSenseClient/Features/CWorldVisuals/CWorldVisuals.hpp>

auto Hook_SetShaderParam( __m128i* map, std::uint32_t hash, __m128i* value ) -> std::uintptr_t
{
	GetWorldVisuals()->on_set_shader_param( value, hash );

	return SetShaderParam_o( map, hash, value );
}
