#include "CWorldVisuals.hpp"

#include <cmath>
#include <numbers>

#include <Common/Common.hpp>
#include <PluginSenseClient/Settings/MenuState.hpp>
#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/FunctionListSDK.hpp>
#include <CS2/SDK/Math/Math.hpp>
#include <CS2/SDK/Math/Vector3.hpp>
#include <CS2/SDK/Update/GameTrace.hpp>
#include <CS2/SDK/Update/CCSGOInput.hpp>
#include <GameClient/CL_Players.hpp>

namespace
{
	namespace shader_hash
	{
		constexpr std::uint32_t dof_ranges{ 0x2ACAB07C };
		constexpr std::uint32_t wind_direction{ 0x2A416C12 };
		constexpr std::uint32_t wind_strength_frequency{ 0xEB0D997E };
		constexpr std::uint32_t bloom_scale{ 0x565EAF76 };
		constexpr std::uint32_t bloom_threshold{ 0xBA98A9B0 };
		constexpr std::uint32_t bloom_width{ 0x2AE72B37 };
		constexpr std::uint32_t bloom_strength{ 0xB692902E };
		constexpr std::uint32_t bloom_skybox{ 0x1313A424 };
		constexpr std::uint32_t gamma_hash{ 0x24470A87 };
		constexpr std::uint32_t rain_exposure_to_sky{ 0x374C1B3C };
		constexpr std::uint32_t rain_wetness{ 0x0F592812 };
		constexpr std::uint32_t rain_timer{ 0x2DBEE393 };
	}
}

static CWorldVisuals g_WorldVisuals{};

void CWorldVisuals::on_create_move( class CCSGOInput* pInput )
{
	const auto& sc = menu_state::worldScene;
	if ( !sc.dof || !sc.dofFocus )
		return;

	auto* pPawn = GetCL_Players()->GetLocalPlayerPawn();
	if ( !pPawn )
		return;

	const auto vStart = GetCL_Players()->GetLocalEyeOrigin();

	QAngle ViewAngles = *CCSGOInput_GetViewAngles( pInput, 0 );
	Vector3 vForward;
	Math::AngleVectors( ViewAngles, vForward );
	vForward *= 8192.f;

	const auto vEnd = vStart + vForward;

	Ray_t Ray;
	CGameTrace GameTrace;
	CTraceFilter Filter( 0x1C1003, pPawn, 3, 15 );

	if ( IGamePhysicsQuery_TraceShape( SDK::Pointers::CVPhys2World(), Ray, vStart, vEnd, &Filter, &GameTrace ) )
	{
		if ( GameTrace.pHitEntity )
		{
			const auto hitDist = ( GameTrace.vecEnd - vStart ).Length();
			if ( hitDist > 1.0f )
				m_focus_depth = hitDist;
		}
	}
}

void CWorldVisuals::on_set_shader_param( __m128i*& value, std::uint32_t hash )
{
	static __m128 dof_val;
	static __m128 wind_direction_val;
	static __m128 wind_strength_frequency_val;
	static __m128 bloom_scale_val;
	static __m128 bloom_threshold_val;
	static __m128 bloom_width_val;
	static __m128 bloom_strength_val;
	static __m128 bloom_skybox_val;
	static __m128 gamma_val;
	static __m128 wetness_sky_val;
	static __m128 wetness_density_val;
	static __m128 wetness_timer_val;

	const auto& sc = menu_state::worldScene;
	const auto& ww = menu_state::worldWeather;

	if ( sc.dof && hash == shader_hash::dof_ranges )
		{
			float nb = sc.dofNearBlurry;
			float nc = sc.dofNearCrisp;
			float fc = sc.dofFarCrisp;
			float fb = sc.dofFarBlurry;

			if ( sc.dofFocus && m_focus_depth > 1.0f )
			{
				const float d = m_focus_depth;
				// Ensure crosshair distance stays inside the clear zone
				if ( d < nc ) { nc = d * 0.9f; if ( nb > nc ) nb = nc * 0.5f; }
				if ( d > fc ) { fc = d * 1.1f; if ( fb < fc ) fb = fc * 1.5f; }
			}

			dof_val = _mm_set_ps( fb, fc, nc, nb );
			value = reinterpret_cast<__m128i*>( &dof_val );
		}

	if ( ww.wind )
	{
		if ( hash == shader_hash::wind_direction )
		{
			const auto direction = ww.windDirection *
				( std::numbers::pi_v<float> / 180.0f );
			wind_direction_val = _mm_set_ps(
				0.0f, 0.0f, sinf( direction ), cosf( direction ) );
			value = reinterpret_cast<__m128i*>( &wind_direction_val );
		}
		else if ( hash == shader_hash::wind_strength_frequency )
		{
			wind_strength_frequency_val = _mm_set_ps(
				ww.windTurbulence, ww.windStrength,
				ww.windTurbulence, ww.windStrength );
			value = reinterpret_cast<__m128i*>( &wind_strength_frequency_val );
		}
	}

	if ( sc.bloom )
	{
		const auto t = sc.bloomValue;

		if ( hash == shader_hash::bloom_scale )
		{
			bloom_scale_val = _mm_set_ps1( 0.3f + t * 1.2f );
			value = reinterpret_cast<__m128i*>( &bloom_scale_val );
		}
		else if ( hash == shader_hash::bloom_threshold )
		{
			bloom_threshold_val = _mm_set_ps1( 1.5f - t * 1.2f );
			value = reinterpret_cast<__m128i*>( &bloom_threshold_val );
		}
		else if ( hash == shader_hash::bloom_width )
		{
			bloom_width_val = _mm_set_ps1( 0.5f + t * 1.5f );
			value = reinterpret_cast<__m128i*>( &bloom_width_val );
		}
		else if ( hash == shader_hash::bloom_strength )
		{
			bloom_strength_val = _mm_set_ps1( 0.2f + t * 0.6f );
			value = reinterpret_cast<__m128i*>( &bloom_strength_val );
		}
		else if ( hash == shader_hash::bloom_skybox )
		{
			bloom_skybox_val = _mm_set_ps1( 0.1f + t * 0.4f );
			value = reinterpret_cast<__m128i*>( &bloom_skybox_val );
		}
	}

	if ( sc.gamma && hash == shader_hash::gamma_hash )
	{
		gamma_val = _mm_set_ps1( sc.gammaValue );
		value = reinterpret_cast<__m128i*>( &gamma_val );
	}

	if ( ww.wetness )
	{
		if ( hash == shader_hash::rain_exposure_to_sky )
		{
			wetness_sky_val = _mm_set_ps1( 1.0f );
			value = reinterpret_cast<__m128i*>( &wetness_sky_val );
		}
		else if ( hash == shader_hash::rain_wetness )
		{
			wetness_density_val = _mm_set_ps1( ww.wetnessDensity );
			value = reinterpret_cast<__m128i*>( &wetness_density_val );
		}
		else if ( hash == shader_hash::rain_timer )
		{
			const auto seconds = static_cast<float>( GetTickCount64() ) * 0.001f;
			wetness_timer_val = _mm_set_ps1( seconds * ww.wetnessSpeed );
			value = reinterpret_cast<__m128i*>( &wetness_timer_val );
		}
	}
}

auto GetWorldVisuals() -> CWorldVisuals*
{
	return &g_WorldVisuals;
}
