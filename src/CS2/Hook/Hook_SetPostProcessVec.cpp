#include "Hook_SetPostProcessVec.hpp"

#include <PluginSenseClient/Features/CWorldVisuals/CWorldVisuals.hpp>

auto Hook_SetPostProcessVec( __m128i* map, std::uint32_t hash, __m128i* value ) -> std::uintptr_t
{
	constexpr std::uint32_t dof_ranges{ 0x2ACAB07C };

	// Force-inject DofRanges before any post-process vector when DOF is enabled
	if ( hash != dof_ranges )
	{
		__m128i* dof_value = nullptr;
		GetWorldVisuals()->on_set_shader_param( dof_value, dof_ranges );

		if ( dof_value )
			SetPostProcessVec_o( map, dof_ranges, dof_value );
	}

	GetWorldVisuals()->on_set_shader_param( value, hash );

	return SetPostProcessVec_o( map, hash, value );
}
