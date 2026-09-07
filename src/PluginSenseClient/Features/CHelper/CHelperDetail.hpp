#pragma once

// ============================================================================
// CHelper 内部共享工具(原 CHelper.cpp 匿名命名空间,拆分翻译单元后转 inline)
// 仅限 CHelper 各 .cpp 引用,勿对外暴露。
// ============================================================================

#include <Common/Common.hpp>

#include <cmath>
#include <algorithm>
#include <cctype>
#include <cstdio>
#include <chrono>
#include <string>
#include <unordered_map>

#include <DllLauncher.hpp>

#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/Interface/IEngineToClient.hpp>
#include <CS2/SDK/Interface/IEngineCvar.hpp>
#include <CS2/SDK/Update/CGlobalVarsBase.hpp>
#include <CS2/SDK/Update/CCSGOInput.hpp>
#include <CS2/SDK/FunctionListSDK.hpp>
#include <CS2/SDK/Types/CEntityData.hpp>
#include <CS2/SDK/Math/Math.hpp>
#include <CS2/SDK/Econ/CEconItemDefinition.hpp>

#include <GameClient/CL_Players.hpp>
#include <GameClient/CL_Weapons.hpp>

#include <PluginSenseClient/GUI/framework_w/render/fonts/weapon_icon_map.hpp>

#include "CHelper.hpp"

namespace helper_detail
{
	inline const std::unordered_map<int , const char*> kItemDefinitionNames =
	{
		{ 43 , "weapon_flashbang" } , { 44 , "weapon_hegrenade" } , { 45 , "weapon_smokegrenade" } ,
		{ 46 , "weapon_molotov" } , { 47 , "weapon_decoy" } , { 48 , "weapon_incgrenade" } ,
	};

	inline float WrapYaw( float yaw )
	{
		yaw = std::fmod( yaw + 180.f , 360.f );
		if ( yaw < 0.f )
			yaw += 360.f;
		return yaw - 180.f;
	}

	inline float AngleError( const QAngle& viewAngles , float pitch , float yaw )
	{
		const float dx = pitch - viewAngles.m_x;
		const float dy = WrapYaw( yaw - viewAngles.m_y );
		return std::sqrtf( dx * dx + dy * dy );
	}

	inline std::string NormalizeMapName( const std::string& in )
	{
		std::string name = in;
		std::transform( name.begin() , name.end() , name.begin() , []( unsigned char c )
		{
			return static_cast<char>( std::tolower( c ) );
		} );
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

	inline std::string WeaponDefinitionName( int defIndex )
	{
		const auto it = kItemDefinitionNames.find( defIndex );
		if ( it != kItemDefinitionNames.end() )
			return it->second;
		return "";
	}

	// 原生注入(NtUserInjectMouseInput / NtUserInjectKeyboardInput),
	inline bool InjectMouse( int dx , int dy , DWORD flags )
	{
		static auto inject = []() -> int ( __stdcall* )( void* , int )
		{
			HMODULE lib = GetModuleHandleW( L"win32u.dll" );
			return lib ? reinterpret_cast<int ( __stdcall* )( void* , int )>(
				GetProcAddress( lib , "NtUserInjectMouseInput" ) ) : nullptr;
		}();
		if ( !inject )
			return false;

		struct MousePacket
		{
			POINT point;
			DWORD mouse_data;
			DWORD flags;
			DWORD time;
			ULONG_PTR extra_info;
		} packet{};

		packet.point = { dx , dy };
		packet.mouse_data = 0;
		packet.flags = flags;
		return inject( &packet , 1 ) != FALSE;
	}

	// 角度差 → 鼠标 counts(灵敏度 500ms 缓存 + FOV 补偿 + dither 累计)
	inline bool MouseDeltaCounts( float dPitch , float dYaw , int& dx , int& dy )
	{
		constexpr float kMousemoveYaw{ 0.022f };
		static float s_cachedSensitivity = 2.5f;
		static auto s_lastCheck = std::chrono::steady_clock::time_point{};

		const auto now = std::chrono::steady_clock::now();
		if ( now - s_lastCheck >= std::chrono::milliseconds( 500 ) )
		{
			s_lastCheck = now;
			if ( auto* pCvar = SDK::Interfaces::EngineCvar() )
			{
				if ( auto* convar = pCvar->Find( "sensitivity" ) )
				{
					if ( convar->nType == EConVarType_Float32 )
						s_cachedSensitivity = convar->value.fl;
				}
			}
		}

		float fovAdjust = 1.f;
		if ( auto* player = GetCL_Players()->GetLocalPlayerPawn() )
			fovAdjust = player->m_flFOVSensitivityAdjust();

		const float degPerCount = s_cachedSensitivity * kMousemoveYaw * fovAdjust;
		if ( degPerCount <= 0.f )
			return false;

		static float accX = 0.f , accY = 0.f;
		accX += -dYaw / degPerCount;  // yaw 增大 → 鼠标左移
		accY += dPitch / degPerCount; // pitch 增大(抬头) → 鼠标上移
		dx = static_cast<int>( accX );
		dy = static_cast<int>( accY );
		accX -= static_cast<float>( dx );
		accY -= static_cast<float>( dy );
		return true;
	}

	inline bool InjectKey( int vk , bool pressed )
	{
		static auto inject = []() -> int ( __stdcall* )( void* , int )
		{
			HMODULE lib = GetModuleHandleW( L"win32u.dll" );
			return lib ? reinterpret_cast<int ( __stdcall* )( void* , int )>(
				GetProcAddress( lib , "NtUserInjectKeyboardInput" ) ) : nullptr;
		}();
		if ( !inject )
			return false;

		struct KeyPacket
		{
			WORD virtual_key;
			WORD scan_code;
			DWORD flags;
			DWORD time;
			ULONG_PTR extra_info;
		} packet{};

		packet.virtual_key = static_cast<WORD>( vk );
		packet.scan_code = static_cast<WORD>( MapVirtualKeyW( vk , MAPVK_VK_TO_VSC ) );
		packet.flags = pressed ? 0u : KEYEVENTF_KEYUP;
		return inject( &packet , 1 ) != FALSE;
	}

	inline void SimulateKey( int vk , bool pressed )
	{
		InjectKey( vk , pressed );
	}

	inline void SimulateMouseButton( DWORD downFlag , DWORD upFlag , bool pressed )
	{
		InjectMouse( 0 , 0 , pressed ? downFlag : upFlag );
	}

	inline std::uint32_t TickCount()
	{
		auto* gv = SDK::Pointers::GlobalVarsBase();
		return gv ? static_cast<std::uint32_t>( gv->m_nTickCount() ) : 0u;
	}

	// ---- 穿点(wallbang)武器工具 ----
	// 当前手持武器短名(ak47 / awp / m4a1_silencer ...),用于墙点图标;无则空
	inline std::string WeaponShortName( std::uintptr_t item )
	{
		auto* def = reinterpret_cast<CEconItemDefinition*>( item );
		if ( !def )
			return {};
		const char* raw = def->m_pszWeaponName();
		if ( !raw )
			return {};
		std::string s = raw;
		if ( s.rfind( "weapon_" , 0 ) == 0 )
			s.erase( 0 , 7 );
		return s;
	}

	// 当前手持武器的 esp_icons 图标字符(按 weapon_icon_map),无则空
	// (原文件即依赖 weapon_icon_map,依赖方向未变)
	inline std::string WeaponIconChar( const std::string& shortName )
	{
		if ( shortName.empty() )
			return {};
		const auto it = weapon_icon_map::icon_table.find( shortName );
		return it != weapon_icon_map::icon_table.end() ? it->second : std::string();
	}

	// 从当前 active weapon 取 item(CEconItemDefinition*),穿点武器工具用
	inline std::uintptr_t ActiveWeaponItem()
	{
		auto* weapon = GetCL_Weapons()->GetLocalActiveWeapon();
		if ( !weapon )
			return 0;
		auto* attr = weapon->m_AttributeManager();
		if ( !attr )
			return 0;
		auto* item = attr->m_Item();
		if ( !item )
			return 0;
		return reinterpret_cast<std::uintptr_t>( item->GetStaticData() );
	}

	// 当前手持是否"可穿墙枪械"(手枪~机枪,非雷/刀/C4/电击枪)
	inline bool CurrentWeaponIsWallbang()
	{
		const auto type = GetCL_Weapons()->GetLocalWeaponType();
		switch ( type )
		{
		case CSWeaponType_t::WEAPONTYPE_PISTOL:
		case CSWeaponType_t::WEAPONTYPE_SUBMACHINEGUN:
		case CSWeaponType_t::WEAPONTYPE_RIFLE:
		case CSWeaponType_t::WEAPONTYPE_SHOTGUN:
		case CSWeaponType_t::WEAPONTYPE_SNIPER_RIFLE:
		case CSWeaponType_t::WEAPONTYPE_MACHINEGUN:
			return true;
		default:
			return false;
		}
	}

	// 墙点武器集合存储为逗号分隔的武器短名(如 "ak47,awp")。工具:
	// 解析列表是否包含某把枪
	inline bool WallbangWeaponsContain( const std::string& list , const std::string& shortName )
	{
		if ( list.empty() || shortName.empty() )
			return list.empty(); // 空列表 = 任意枪
		std::size_t start = 0;
		while ( start <= list.size() )
		{
			const auto comma = list.find( ',' , start );
			const auto part = comma == std::string::npos
				? list.substr( start ) : list.substr( start , comma - start );
			if ( part == shortName )
				return true;
			if ( comma == std::string::npos )
				break;
			start = comma + 1;
		}
		return false;
	}

	// 执行控制台命令(经 InputService,与 CNameChanger::RunCommand 同机制)
	inline void RunConsoleCommand( const char* cmd )
	{
		static void* s_input = nullptr;
		static void( __fastcall** s_exec )( void*, int, const char*, int ) = nullptr;
		if ( !s_input )
		{
			HMODULE eng = GetModuleHandleW( L"engine2.dll" );
			if ( !eng )
				return;
			auto create = ( void* ( __cdecl* )( const char*, int* ) )GetProcAddress( eng , "CreateInterface" );
			s_input = create ? create( "InputService_001" , nullptr ) : nullptr;
			if ( s_input )
			{
				void** vt = *( void*** )s_input;
				s_exec = ( void( __fastcall** )( void*, int, const char*, int ) )&vt[ 25 ];
			}
		}
		if ( s_input && s_exec )
			( *s_exec )( s_input , 5 , cmd , 0 );
	}

	inline std::chrono::steady_clock::time_point Now()
	{
		return std::chrono::steady_clock::now();
	}

	inline bool GameHasInputFocus()
	{
		const auto foreground = ::GetForegroundWindow();
		const auto root = foreground ? ::GetAncestor( foreground , GA_ROOT ) : nullptr;
		DWORD processId{};
		if ( root )
			::GetWindowThreadProcessId( root , &processId );
		return processId != 0 && processId == ::GetCurrentProcessId();
	}

	// ==================== 玩家按键绑定读取 ====================
	inline bool SafeCopy( void* dst , std::uintptr_t src , std::size_t size )
	{
		if ( !src || src < 0x10000 )
			return false;
		MEMORY_BASIC_INFORMATION mbi{};
		if ( VirtualQuery( reinterpret_cast<void*>( src ) , &mbi , sizeof( mbi ) ) == 0 )
			return false;
		if ( mbi.State != MEM_COMMIT )
			return false;
		__try
		{
			memcpy( dst , reinterpret_cast<void*>( src ) , size );
			return true;
		}
		__except ( EXCEPTION_EXECUTE_HANDLER )
		{
			return false;
		}
	}

	// 安全读实体上的 schema 字段:地址/偏移/内存任一不可靠时返回 fallback,不裸解引用
	inline bool SafeReadSchemaBool( std::uintptr_t entity , const char* className , const char* propertyName , bool fallback )
	{
		if ( !entity || entity < 0x10000 )
			return fallback;

		auto* schema = GetSchemaOffset();
		if ( !schema )
			return fallback;

		const uint32_t offset = schema->GetOffset( className , propertyName );
		if ( offset == 0 || offset > 0x10000 )
			return fallback;

		bool value = fallback;
		if ( !SafeCopy( &value , entity + offset , sizeof( value ) ) )
			return fallback;
		return value;
	}

	// 把用户配置的 VK 码转成内部绑定(鼠标键优先)
	inline InputBinding VkToBinding( int vk )
	{
		if ( vk == VK_LBUTTON ) return { InputDevice::MousePrimary , 0 };
		if ( vk == VK_RBUTTON ) return { InputDevice::MouseSecondary , 0 };
		if ( vk == VK_MBUTTON ) return { InputDevice::MouseMiddle , 0 };
		if ( vk == VK_XBUTTON1 ) return { InputDevice::MouseAux1 , 0 };
		if ( vk == VK_XBUTTON2 ) return { InputDevice::MouseAux2 , 0 };
		if ( vk > 0 && vk <= 255 ) return { InputDevice::Keyboard , vk };
		return {};
	}
}

// 各翻译单元统一引用 helper_detail:: 而非裸名
using namespace helper_detail;
