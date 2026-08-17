#include "CWeather.hpp"
#include "weather_particles.hpp"

#include <cmath>
#include <numbers>

#include <Common/Common.hpp>
#include <PluginSenseClient/Settings/MenuState.hpp>
#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/FunctionListSDK.hpp>
#include <CS2/SDK/Math/Vector3.hpp>
#include <GameClient/CL_Players.hpp>

namespace
{
	struct particle_transform
	{
		float px{};
		float py{};
		float pz{};
		float pw{};
		float qx{};
		float qy{};
		float qz{};
		float qw{ 1.0f };
	};
	static_assert( sizeof( particle_transform ) == 0x20 );

	// Schema offsets from SchemaDump.hpp
	constexpr std::uint32_t offset_round_start_time = 0x0070; // C_CSGameRules::m_fRoundStartTime

	// Safe memory read — mirrors velocity's memory::read<T> pattern
	template<typename T>
	T safe_read( std::uintptr_t address, const T& default_value = {} ) noexcept
	{
		__try
		{
			return *reinterpret_cast<T*>( address );
		}
		__except ( EXCEPTION_EXECUTE_HANDLER )
		{
			return default_value;
		}
	}

	template<typename T>
	T safe_read( void* address, const T& default_value = {} ) noexcept
	{
		return safe_read<T>( reinterpret_cast<std::uintptr_t>( address ), default_value );
	}
}

static CWeather g_Weather{};

void CWeather::Init()
{
}

void CWeather::on_frame_stage_notify()
{
	if ( !menu_state::worldWeather.enabled )
	{
		if ( m_effect_index != invalid_effect_index )
			release_particles();

		m_last_particle_type = -1;
		return;
	}

	const auto game_rules = SDK::Pointers::GameRules();
	if ( !game_rules )
	{
		release_particles();
		return;
	}

	// Round restart detection
	const auto round_start_time = safe_read<float>(
		reinterpret_cast<std::uintptr_t>( game_rules ) + offset_round_start_time, 0.0f );
	if ( round_start_time != m_last_round_start_time )
	{
		release_particles();
		m_last_round_start_time = round_start_time;
	}

	update_particles();
}

void CWeather::release()
{
	release_particles();
}

void CWeather::create_particle()
{
	const auto particle_manager = SDK::Pointers::ParticleManager();
	if ( !particle_manager )
		return;

	const auto type = menu_state::worldWeather.type;
	if ( type < 0 || type > 2 )
		return;

	static const char* particle_paths[] = {
		"particles/embedded/snow.vpcf",
		"particles/embedded/rain.vpcf",
		"particles/embedded/stars.vpcf"
	};

	const std::string particle_path = particle_paths[type];

	buffer_string buffer{};
	buffer.m_unknown2 = 0xc00000c8;

	InitParticlePathBuffer( &buffer, particle_path.c_str() );
	buffer.m_unknown4 = 'fcpv';

	const auto resource_system = SDK::Interfaces::ResourceSystem();
	if ( resource_system )
		ResourceSystemLoad( resource_system, &buffer, "" );

	auto effect_index = invalid_effect_index;
	ParticleCreateEffect( particle_manager, reinterpret_cast<int*>( &effect_index ),
		particle_path.c_str(), 8, 0, 0, 0, 0 );

	m_effect_index = effect_index;
	m_last_particle_type = type;
}

void CWeather::update_particles()
{
	const auto current_type = menu_state::worldWeather.type;

	if ( m_effect_index != invalid_effect_index && m_last_particle_type != current_type )
		release_particles();

	if ( m_effect_index == invalid_effect_index )
	{
		create_particle();
		if ( m_effect_index == invalid_effect_index )
			return;
	}

	const auto particle_manager = SDK::Pointers::ParticleManager();
	if ( !particle_manager )
		return;

	// Use the existing safe CL_Players interface to get local origin
	const Vector3 origin = GetCL_Players()->GetLocalOrigin();

	// Wind tilt for rain particles
	if ( current_type == 1 && menu_state::worldWeather.wind )
	{
		const auto direction = menu_state::worldWeather.windDirection * ( std::numbers::pi_v<float> / 180.0f );

		const auto raw_strength = menu_state::worldWeather.windStrength / 5.0f;
		const auto strength = ( raw_strength < 0.0f ) ? 0.0f : ( ( raw_strength > 1.0f ) ? 1.0f : raw_strength );

		const auto raw_turbulence = menu_state::worldWeather.windTurbulence / 5.0f;
		const auto turbulence = ( raw_turbulence < 0.0f ) ? 0.0f : ( ( raw_turbulence > 1.0f ) ? 1.0f : raw_turbulence );

		float vx = strength * std::cos( direction );
		float vy = strength * std::sin( direction );

		if ( turbulence > 0.0f )
		{
			const auto time = static_cast<double>( GetTickCount64() ) * 0.0006;
			const auto noise_x = static_cast<float>(
				0.60f * std::sin( time * 0.90f + 0.3f ) +
				0.30f * std::sin( time * 2.30f + 1.7f ) +
				0.15f * std::sin( time * 5.10f + 4.2f ) );
			const auto noise_y = static_cast<float>(
				0.60f * std::sin( time * 1.10f + 2.0f ) +
				0.30f * std::sin( time * 2.70f + 0.5f ) +
				0.15f * std::sin( time * 4.60f + 3.1f ) );
			vx += turbulence * 0.9f * noise_x;
			vy += turbulence * 0.9f * noise_y;
		}

		const auto speed = std::sqrt( vx * vx + vy * vy );
		const auto magnitude = ( speed < 1.0f ) ? speed : 1.0f;
		const auto tilt = magnitude * 80.0f * ( std::numbers::pi_v<float> / 180.0f );
		const auto heading = vx != 0.0f || vy != 0.0f ? std::atan2( vy, vx ) : 0.0f;
		const auto sine = std::sin( tilt * 0.5f );

		const particle_transform transform{
			origin.m_x, origin.m_y, origin.m_z, 0.0f,
			std::sin( heading ) * sine,
			-std::cos( heading ) * sine,
			0.0f,
			std::cos( tilt * 0.5f )
		};

		ParticleSetTransform( particle_manager, m_effect_index, 0,
			const_cast<particle_transform*>( &transform ), 0 );
	}
	else
	{
		ParticleSetControlPoint( particle_manager, m_effect_index, 0,
			const_cast<Vector3*>( &origin ), 0 );
	}

	// Set particle color from settings
	const auto& col = menu_state::worldWeather.color;
	const Vector3 color{ col.x * 255.f, col.y * 255.f, col.z * 255.f };
	ParticleSetControlPoint( particle_manager, m_effect_index, 1,
		const_cast<Vector3*>( &color ), 0 );
}

void CWeather::release_particles()
{
	if ( m_effect_index == invalid_effect_index )
		return;

	const auto particle_manager = SDK::Pointers::ParticleManager();
	if ( particle_manager )
		ParticleDestroyEffect( particle_manager, m_effect_index, true, true );

	m_effect_index = invalid_effect_index;
	m_last_particle_type = -1;
	m_particle_loaded = false;
}

auto GetWeather() -> CWeather*
{
	return &g_Weather;
}
