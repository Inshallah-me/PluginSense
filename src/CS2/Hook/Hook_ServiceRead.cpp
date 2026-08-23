#include "Hook_ServiceRead.hpp"

#include <cstring>
#include <span>

#include <Common/Common.hpp>
#include <Common/MemoryEngine.hpp>
#include <PluginSenseClient/Features/CWeather/weather_particles.hpp>

auto Hook_ServiceRead( std::uintptr_t a1 ) -> std::uintptr_t
{
	const auto flags_len = *( std::uint32_t* )( a1 - 212 );
	const auto len = flags_len & 0x3FFFFFFF;

	std::string filename;
	if ( len > 0 && len < 512 )
	{
		char buffer[ 512 ]{};

		if ( flags_len & 0x40000000 )
		{
			const auto copy_len = ( len < 511u ) ? len : 511u;
			std::memcpy( buffer, reinterpret_cast< void* >( a1 - 208 ), copy_len );
		}
		else
		{
			const auto string = *( std::uintptr_t* )( a1 - 208 );
			if ( string )
			{
				const auto copy_len = ( len < 511u ) ? len : 511u;
				std::memcpy( buffer, reinterpret_cast< void* >( string ), copy_len );
			}
		}

		filename = buffer;
	}

	if ( filename.find( "particles/embedded/" ) != std::string::npos )
	{
		std::span<const unsigned char> particle_data;

		if ( filename.find( "snow" ) != std::string::npos )
			particle_data = resources::particles::weather::snow;
		else if ( filename.find( "rain" ) != std::string::npos )
			particle_data = resources::particles::weather::rain;
		else if ( filename.find( "stars" ) != std::string::npos )
			particle_data = resources::particles::weather::stars;
		else if ( filename.find( "sparks" ) != std::string::npos )
			particle_data = resources::particles::effects::sparks;
		else if ( filename.find( "fade" ) != std::string::npos )
			particle_data = resources::particles::effects::fade;

		if ( !particle_data.empty() )
		{
			const auto async_filesystem = *( std::uintptr_t* )( a1 + 24 );
			if ( async_filesystem )
			{
				// vfunc index 22: allocate buffer in async filesystem
				using alloc_fn = std::uintptr_t( __fastcall* )( void*, std::size_t, const char* );
				const auto alloc = ( *(alloc_fn**)( async_filesystem ) )[ 22 ];
				const auto buf = alloc( (void*)async_filesystem, particle_data.size(), filename.c_str() );

				if ( buf )
				{
					std::memcpy( reinterpret_cast< void* >( buf ), particle_data.data(), particle_data.size() );

					*( std::uintptr_t* )( a1 + 56 ) = buf;
					*( std::uintptr_t* )( a1 + 64 ) = particle_data.size();
					*( std::uintptr_t* )( a1 + 72 ) = particle_data.size();

					// filesystem_close: pattern >E8????????FFD34C8BA42498000000 in filesystem_stdio.dll
					static auto close_fn = reinterpret_cast<void(__fastcall*)(std::uintptr_t, int)>(
						GetCallAddress( reinterpret_cast<intptr_t>(
							FindPattern( "filesystem_stdio.dll",
								"E8 ? ? ? ? FF D3 4C 8B A4 24 98 00 00 00" ) ) ) );
					if ( close_fn )
						close_fn( a1 - 224, 0 );

					return 0;
				}
			}
		}
	}

	return ServiceRead_o( a1 );
}
