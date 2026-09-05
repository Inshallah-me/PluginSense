#include "HelperTimeline.hpp"

#include <Common/DevLog.hpp>

#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/Update/CUserCmd.hpp>
#include <CS2/SDK/Econ/CEconItemDefinition.hpp>

#include "HelperTimelineData.hpp"

#include <algorithm>
#include <unordered_set>

namespace helper_timeline
{
	namespace
	{
		// kind 值与 resources::nades::kind 对齐(smoke0/flash1/molotov2/he3/decoy4)
		constexpr std::uint8_t kKindSmoke = 0;
		constexpr std::uint8_t kKindFlash = 1;
		constexpr std::uint8_t kKindMolotov = 2;
		constexpr std::uint8_t kKindHe = 3;
		constexpr std::uint8_t kKindDecoy = 4;
		constexpr std::uint8_t kKindInvalid = 0xFF;

		struct Library
		{
			std::unordered_map<std::string , std::vector<Point>> maps;
			int pointCount = 0;
		};

		Library g_Library;
		std::atomic_bool g_Ready{ false };

		std::string ToLower( std::string s )
		{
			std::transform( s.begin() , s.end() , s.begin() , []( unsigned char c )
			{
				return static_cast<char>( std::tolower( c ) );
			} );
			return s;
		}

		std::string NormalizeMapName( std::string name )
		{
			name = ToLower( std::move( name ) );
			for ( auto& c : name )
				if ( c == '\\' )
					c = '/';
			const auto dot = name.find( '.' );
			if ( dot != std::string::npos )
				name = name.substr( 0 , dot );
			const auto slash = name.find_last_of( '/' );
			if ( slash != std::string::npos )
				name = name.substr( slash + 1 );
			return name;
		}

		float NormalizeYaw( float yaw )
		{
			yaw = std::fmod( yaw + 180.f , 360.f );
			if ( yaw < 0.f )
				yaw += 360.f;
			return yaw - 180.f;
		}

		std::string WeaponDefinitionName( int defIndex )
		{
			switch ( defIndex )
			{
			case 43: return "weapon_flashbang";
			case 44: return "weapon_hegrenade";
			case 45: return "weapon_smokegrenade";
			case 46: return "weapon_molotov";
			case 47: return "weapon_decoy";
			case 48: return "weapon_incgrenade";
			default: return {};
			}
		}

		std::uint8_t WeaponToKind( const std::string& weapon )
		{
			if ( weapon == "weapon_smokegrenade" ) return kKindSmoke;
			if ( weapon == "weapon_flashbang" ) return kKindFlash;
			if ( weapon == "weapon_molotov" || weapon == "weapon_incgrenade" ) return kKindMolotov;
			if ( weapon == "weapon_hegrenade" ) return kKindHe;
			if ( weapon == "weapon_decoy" ) return kKindDecoy;
			return kKindInvalid;
		}

		// 从编译期嵌入表构建运行时库(字段解码在生成期已完成)
		void LoadEmbeddedLibrary( const std::unordered_set<int>& hiddenIds )
		{
			Library library;
			int serial = 0;

			for ( std::size_t p = 0; p < helper_timeline_data::kPointCount; ++p )
			{
				const auto& pd = helper_timeline_data::kPoints[ p ];
				const std::string mapName = NormalizeMapName( helper_timeline_data::kMapNames[ pd.mapId ] );

				Point point;
				point.id = ++serial;
				point.name = pd.name;
				point.weapon = WeaponDefinitionName( pd.weaponId );
				point.kind = WeaponToKind( point.weapon );
				if ( point.kind == kKindInvalid )
					continue;
				point.position = Vector3( pd.x , pd.y , pd.z );
				point.angles = QAngle( pd.pitch , NormalizeYaw( pd.yaw ) , 0.f );

				point.frames.reserve( pd.frameCount );
				for ( std::uint32_t f = 0; f < pd.frameCount; ++f )
				{
					const auto& fd = helper_timeline_data::kFrames[ pd.frameOffset + f ];
					Frame frame;
					frame.angles = QAngle( fd.pitch , NormalizeYaw( fd.yaw ) , 0.f );
					frame.position = Vector3( fd.x , fd.y , fd.z );
					frame.in_attack    = ( fd.buttons & helper_timeline_data::btn_attack ) != 0;
					frame.in_attack2   = ( fd.buttons & helper_timeline_data::btn_attack2 ) != 0;
					frame.in_jump      = ( fd.buttons & helper_timeline_data::btn_jump ) != 0;
					frame.in_duck      = ( fd.buttons & helper_timeline_data::btn_duck ) != 0;
					frame.in_forward   = ( fd.buttons & helper_timeline_data::btn_forward ) != 0;
					frame.in_back      = ( fd.buttons & helper_timeline_data::btn_back ) != 0;
					frame.in_use       = ( fd.buttons & helper_timeline_data::btn_use ) != 0;
					frame.in_moveleft  = ( fd.buttons & helper_timeline_data::btn_moveleft ) != 0;
					frame.in_moveright = ( fd.buttons & helper_timeline_data::btn_moveright ) != 0;
					frame.in_speed     = ( fd.buttons & helper_timeline_data::btn_speed ) != 0;
					point.frames.push_back( frame );
				}

				library.maps[ mapName ].push_back( std::move( point ) );
				++library.pointCount;
			}

			g_Library = std::move( library );

			// 应用用户隐藏覆盖(hiddenIds 由外部从 helper_lineups.dat 收集)
			std::size_t hiddenCount = 0;
			for ( auto& [ mapName , points ] : g_Library.maps )
			{
				for ( auto& point : points )
				{
					const auto it = hiddenIds.find( point.id );
					if ( it != hiddenIds.end() )
					{
						point.hidden = true;
						++hiddenCount;
					}
				}
			}

			g_Ready.store( true , std::memory_order_release );
			DEV_LOG( "[timeline] embedded library ready: maps=%zu points=%d hidden=%zu" ,
				g_Library.maps.size() , g_Library.pointCount , hiddenCount );
		}
	}

	auto StartLoad( const std::unordered_set<int>& hiddenIds ) -> void
	{
		if ( !g_Ready.load( std::memory_order_acquire ) )
			LoadEmbeddedLibrary( hiddenIds );
	}

	auto Ready() -> bool
	{
		return g_Ready.load( std::memory_order_acquire );
	}

	auto GetMapPoints( const std::string& mapName ) -> const std::vector<Point>*
	{
		if ( !Ready() )
			return nullptr;
		const auto it = g_Library.maps.find( ToLower( mapName ) );
		if ( it == g_Library.maps.end() || it->second.empty() )
			return nullptr;
		return &it->second;
	}

	auto ApplyHiddenOverrides( const std::unordered_set<int>& hiddenIds ) -> void
	{
		if ( !Ready() || hiddenIds.empty() )
			return;
		std::size_t count = 0;
		for ( auto& [ mapName , points ] : g_Library.maps )
			for ( auto& point : points )
			{
				point.hidden = hiddenIds.count( point.id ) != 0;
				if ( point.hidden )
					++count;
			}
		DEV_LOG( "[timeline] hidden overrides applied: %zu" , count );
	}

	auto FindPointById( int id ) -> Point*
	{
		if ( !Ready() )
			return nullptr;
		for ( auto& [ mapName , points ] : g_Library.maps )
			for ( auto& point : points )
				if ( point.id == id )
					return &point;
		return nullptr;
	}
}
