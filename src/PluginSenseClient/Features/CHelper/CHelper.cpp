#include "CHelper.hpp"

#include <cmath>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdio>
#include <numbers>

#include <DllLauncher.hpp>

#include <Common/DevLog.hpp>

#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/Interface/IEngineToClient.hpp>
#include <CS2/SDK/Interface/IEngineCvar.hpp>
#include <CS2/SDK/Update/CGlobalVarsBase.hpp>
#include <CS2/SDK/Update/CCSGOInput.hpp>
#include <CS2/SDK/FunctionListSDK.hpp>
#include <CS2/SDK/Types/CEntityData.hpp>
#include <CS2/SDK/Math/Math.hpp>

#include <PluginSenseClient/Settings/MenuState.hpp>
#include <PluginSenseClient/GUI/framework_w/includes.hh>

#include <GameClient/CL_Players.hpp>
#include <GameClient/CL_Weapons.hpp>

namespace nd = resources::nades;

namespace
{
	const std::unordered_map<int , const char*> kItemDefinitionNames =
	{
		{ 43 , "weapon_flashbang" } , { 44 , "weapon_hegrenade" } , { 45 , "weapon_smokegrenade" } ,
		{ 46 , "weapon_molotov" } , { 47 , "weapon_decoy" } , { 48 , "weapon_incgrenade" } ,
	};

	float WrapYaw( float yaw )
	{
		yaw = std::fmod( yaw + 180.f , 360.f );
		if ( yaw < 0.f )
			yaw += 360.f;
		return yaw - 180.f;
	}

	float AngleError( const QAngle& viewAngles , float pitch , float yaw )
	{
		const float dx = pitch - viewAngles.m_x;
		const float dy = WrapYaw( yaw - viewAngles.m_y );
		return std::sqrtf( dx * dx + dy * dy );
	}

	std::string NormalizeMapName( const std::string& in )
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

	const nd::map_entries* FindMap( const std::string& name )
	{
		for ( const auto& candidate : nd::k_maps )
		{
			if ( name == candidate.map )
				return &candidate;
		}
		return nullptr;
	}

	std::string WeaponDefinitionName( int defIndex )
	{
		const auto it = kItemDefinitionNames.find( defIndex );
		if ( it != kItemDefinitionNames.end() )
			return it->second;
		return "";
	}

	bool IsGrenadeWeapon( const std::string& weapon )
	{
		return weapon == "weapon_smokegrenade" || weapon == "weapon_flashbang" ||
               weapon == "weapon_hegrenade" || weapon == "weapon_molotov" ||
               weapon == "weapon_incgrenade" || weapon == "weapon_decoy";
	}

	ImU32 HelperKindColor( std::uint8_t kind , float alpha )
	{
		const int a = static_cast<int>( std::clamp( alpha , 0.f , 255.f ) );
		switch ( static_cast<nd::kind>( kind ) )
		{
		case nd::kind::smoke:   return IM_COL32( 155 , 180 , 205 , a );
		case nd::kind::flash:   return IM_COL32( 255 , 232 , 105 , a );
		case nd::kind::molotov: return IM_COL32( 255 , 145 , 55 , a );
		case nd::kind::he:      return IM_COL32( 255 , 105 , 105 , a );
		case nd::kind::decoy:   return IM_COL32( 175 , 135 , 255 , a );
		default:                return IM_COL32( 110 , 185 , 255 , a );
		}
	}

	// 原生注入(NtUserInjectMouseInput / NtUserInjectKeyboardInput),
	bool InjectMouse( int dx , int dy , DWORD flags )
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

	bool InjectKey( int vk , bool pressed )
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

	void SimulateKey( int vk , bool pressed )
	{
		InjectKey( vk , pressed );
	}

	void SimulateMouseButton( DWORD downFlag , DWORD upFlag , bool pressed )
	{
		InjectMouse( 0 , 0 , pressed ? downFlag : upFlag );
	}

	std::uint32_t TickCount()
	{
		auto* gv = SDK::Pointers::GlobalVarsBase();
		return gv ? static_cast<std::uint32_t>( gv->m_nTickCount() ) : 0u;
	}

	std::chrono::steady_clock::time_point Now()
	{
		return std::chrono::steady_clock::now();
	}

	bool GameHasInputFocus()
	{
		const auto foreground = ::GetForegroundWindow();
		const auto root = foreground ? ::GetAncestor( foreground , GA_ROOT ) : nullptr;
		DWORD processId{};
		if ( root )
			::GetWindowThreadProcessId( root , &processId );
		return processId != 0 && processId == ::GetCurrentProcessId();
	}

	// ==================== 玩家按键绑定读取 ====================
	bool SafeCopy( void* dst , std::uintptr_t src , std::size_t size )
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

	std::string SafeReadString( std::uintptr_t addr , std::size_t maxLen )
	{
		if ( !addr || addr < 0x10000 )
			return {};
		std::vector<char> buffer( maxLen + 1 , 0 );
		if ( !SafeCopy( buffer.data() , addr , maxLen ) )
			return {};
		return std::string( buffer.data() );
	}

	InputBinding SourceKeyToBinding( const std::string& name )
	{
		std::string normalized = name;
		std::transform( normalized.begin() , normalized.end() , normalized.begin() ,
			[]( unsigned char c ) { return static_cast<char>( std::toupper( c ) ); } );

		const auto mouse = []( InputDevice device )
		{
			return InputBinding{ device , 0 };
		};
		if ( normalized == "MOUSE1" ) return mouse( InputDevice::MousePrimary );
		if ( normalized == "MOUSE2" ) return mouse( InputDevice::MouseSecondary );
		if ( normalized == "MOUSE3" ) return mouse( InputDevice::MouseMiddle );
		if ( normalized == "MOUSE4" ) return mouse( InputDevice::MouseAux1 );
		if ( normalized == "MOUSE5" ) return mouse( InputDevice::MouseAux2 );

		std::uint16_t virtualKey{};
		if ( normalized.size() == 1 )
		{
			const auto value = normalized.front();
			if ( ( value >= 'A' && value <= 'Z' ) || ( value >= '0' && value <= '9' ) )
				virtualKey = static_cast<std::uint16_t>( value );
		}
		if ( !virtualKey && normalized.size() >= 2 && normalized.front() == 'F' )
		{
			int number = 0;
			for ( std::size_t i = 1; i < normalized.size(); ++i )
			{
				if ( normalized[i] < '0' || normalized[i] > '9' )
				{
					number = 0;
					break;
				}
				number = number * 10 + normalized[i] - '0';
			}
			if ( number >= 1 && number <= 24 )
				virtualKey = static_cast<std::uint16_t>( VK_F1 + number - 1 );
		}

		if ( !virtualKey )
		{
			static const std::unordered_map<std::string , std::uint16_t> namedKeys =
			{
				{ "SPACE" , VK_SPACE } , { "TAB" , VK_TAB } , { "ENTER" , VK_RETURN } ,
				{ "ESCAPE" , VK_ESCAPE } , { "BACKSPACE" , VK_BACK } , { "CAPSLOCK" , VK_CAPITAL } ,
				{ "NUMLOCK" , VK_NUMLOCK } , { "SCROLLLOCK" , VK_SCROLL } , { "INSERT" , VK_INSERT } ,
				{ "DELETE" , VK_DELETE } , { "HOME" , VK_HOME } , { "END" , VK_END } ,
				{ "PGUP" , VK_PRIOR } , { "PGDN" , VK_NEXT } , { "PAUSE" , VK_PAUSE } ,
				{ "SHIFT" , VK_LSHIFT } , { "RSHIFT" , VK_RSHIFT } , { "CTRL" , VK_LCONTROL } ,
				{ "RCTRL" , VK_RCONTROL } , { "ALT" , VK_LMENU } , { "RALT" , VK_RMENU } ,
				{ "UPARROW" , VK_UP } , { "DOWNARROW" , VK_DOWN } , { "LEFTARROW" , VK_LEFT } ,
				{ "RIGHTARROW" , VK_RIGHT } , { "SEMICOLON" , VK_OEM_1 } , { "SLASH" , VK_OEM_2 } ,
				{ "BACKQUOTE" , VK_OEM_3 } , { "LBRACKET" , VK_OEM_4 } , { "BACKSLASH" , VK_OEM_5 } ,
				{ "RBRACKET" , VK_OEM_6 } , { "APOSTROPHE" , VK_OEM_7 } , { "EQUAL" , VK_OEM_PLUS } ,
				{ "COMMA" , VK_OEM_COMMA } , { "MINUS" , VK_OEM_MINUS } , { "PERIOD" , VK_OEM_PERIOD } ,
			};
			const auto it = namedKeys.find( normalized );
			if ( it != namedKeys.end() )
				virtualKey = it->second;
		}

		return virtualKey ? InputBinding{ InputDevice::Keyboard , virtualKey } : InputBinding{};
	}

	// 把用户配置的 VK 码转成内部绑定(鼠标键优先)
	InputBinding VkToBinding( int vk )
	{
		if ( vk == VK_LBUTTON ) return { InputDevice::MousePrimary , 0 };
		if ( vk == VK_RBUTTON ) return { InputDevice::MouseSecondary , 0 };
		if ( vk == VK_MBUTTON ) return { InputDevice::MouseMiddle , 0 };
		if ( vk == VK_XBUTTON1 ) return { InputDevice::MouseAux1 , 0 };
		if ( vk == VK_XBUTTON2 ) return { InputDevice::MouseAux2 , 0 };
		if ( vk > 0 && vk <= 255 ) return { InputDevice::Keyboard , vk };
		return {};
	}

	bool ContainsCommand( const std::string& command , const std::string& token )
	{
		if ( token.empty() )
			return false;
		const auto delimiter = []( char value )
		{
			return std::isspace( static_cast<unsigned char>( value ) ) != 0
				|| value == ';' || value == '"';
		};
		std::string lower = command;
		std::transform( lower.begin() , lower.end() , lower.begin() ,
			[]( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );
		for ( std::size_t cursor = 0; cursor + token.size() <= lower.size(); ++cursor )
		{
			if ( lower.compare( cursor , token.size() , token ) != 0 )
				continue;
			const bool before = cursor == 0 || delimiter( command[ cursor - 1 ] );
			const bool after = cursor + token.size() == command.size()
				|| delimiter( command[ cursor + token.size() ] );
			if ( before && after )
				return true;
		}
		return false;
	}

	constexpr std::size_t kInputCodeCount{ 0x204 };
	constexpr std::size_t kBindingRecordSize{ 0x58 };
	constexpr std::size_t kBindingSlotCount{ 11 };
	constexpr std::size_t kInputNameBias{ 1 };
	constexpr std::uintptr_t kCurrentBindingOffset{ 0x33050 };
	constexpr std::uintptr_t kCurrentKeyNameTableRva{ 0x3a210 };

	std::uintptr_t GetInputService()
	{
		HMODULE eng = GetModuleHandleW( L"engine2.dll" );
		if ( !eng )
			return 0;
		auto create = reinterpret_cast<void* ( __cdecl* )( const char* , int* )>(
			GetProcAddress( eng , "CreateInterface" ) );
		return create ? reinterpret_cast<std::uintptr_t>( create( "InputService_001" , nullptr ) ) : 0;
	}

	std::uintptr_t BindingMemberOffset( std::uintptr_t getter )
	{
		if ( !getter )
			return kCurrentBindingOffset;
		std::array<std::uint8_t , 96> code{};
		if ( !SafeCopy( code.data() , getter , code.size() ) )
			return kCurrentBindingOffset;
		for ( std::size_t i = 0; i + 8 <= code.size(); ++i )
		{
			if ( ( code[ i ] & 0xf0u ) != 0x40u || code[ i + 1 ] != 0x8bu
				|| ( code[ i + 2 ] & 0xc7u ) != 0x84u )
				continue;
			std::int32_t displacement{};
			memcpy( &displacement , code.data() + i + 4 , sizeof( displacement ) );
			if ( displacement >= 0x10000 && displacement <= 0x100000 )
				return static_cast<std::uintptr_t>( displacement );
		}
		return kCurrentBindingOffset;
	}

	std::uintptr_t LocateKeyNameTable( std::uintptr_t inputModule )
	{
		const auto validate = []( std::uintptr_t table )
		{
			if ( !table || table < 0x10000 )
				return false;
			std::uintptr_t spacePtr = 0;
			if ( !SafeCopy( &spacePtr , table + 66 * sizeof( std::uintptr_t ) , sizeof( spacePtr ) ) )
				return false;
			return SafeReadString( spacePtr , 16 ) == "SPACE";
		};

		if ( !inputModule )
			return 0;
		const auto fallback = inputModule + kCurrentKeyNameTableRva;
		return validate( fallback ) ? fallback : 0;
	}

	struct LiveInputBindings
	{
		std::array<std::vector<InputBinding> , 9> bindings{}; // Forward/Back/Left/Right/Walk/Duck/Jump/Attack/Attack2
		std::uintptr_t inputService = 0;
		std::uintptr_t bindingTable = 0;
		std::uintptr_t keyNameTable = 0;
		std::chrono::steady_clock::time_point nextRefresh{};

		static std::size_t ActionIndex( InputAction action )
		{
			switch ( action )
			{
			case InputAction::Forward: return 0;
			case InputAction::Back:    return 1;
			case InputAction::Left:    return 2;
			case InputAction::Right:   return 3;
			case InputAction::Walk:    return 4;
			case InputAction::Duck:    return 5;
			case InputAction::Jump:    return 6;
			case InputAction::Attack:  return 7;
			case InputAction::Attack2: return 8;
			}
			return 0;
		}

		void RefreshLocked( std::chrono::steady_clock::time_point now )
		{
			if ( now < nextRefresh )
				return;
			nextRefresh = now + std::chrono::seconds( 1 );

			if ( !inputService )
			{
				inputService = GetInputService();
				std::uintptr_t vtable = 0;
				if ( inputService && SafeCopy( &vtable , inputService , sizeof( vtable ) ) && vtable )
				{
					std::uintptr_t getter = 0;
					if ( SafeCopy( &getter , vtable + 32 * sizeof( std::uintptr_t ) , sizeof( getter ) ) )
						bindingTable = inputService + BindingMemberOffset( getter );
				}
			}
			if ( !keyNameTable )
				keyNameTable = LocateKeyNameTable(
					reinterpret_cast<std::uintptr_t>( GetModuleHandleW( L"inputsystem.dll" ) ) );
			if ( !bindingTable || !keyNameTable )
				return;

			std::vector<std::uint8_t> records( kInputCodeCount * kBindingRecordSize );
			std::array<std::uintptr_t , kInputCodeCount> keyNames{};
			if ( !SafeCopy( records.data() , bindingTable , records.size() )
				|| !SafeCopy( keyNames.data() , keyNameTable , sizeof( keyNames ) ) )
				return;

			const auto keyNameIs = [ &keyNames ]( std::size_t code , const std::string& expected )
			{
				return code < keyNames.size() && keyNames[ code ]
					&& SafeReadString( keyNames[ code ] , 32 ) == expected;
			};
			if ( !keyNameIs( 65 , "ENTER" ) || !keyNameIs( 66 , "SPACE" )
				|| !keyNameIs( 317 , "MOUSE1" ) || !keyNameIs( 318 , "MOUSE2" ) )
			{
				bindings = {};
				return;
			}

			std::array<std::vector<InputBinding> , 9> refreshed{};
			for ( std::size_t code = 0; code + kInputNameBias < kInputCodeCount; ++code )
			{
				const auto nameAddress = keyNames[ code + kInputNameBias ];
				if ( !nameAddress )
					continue;
				const std::string name = SafeReadString( nameAddress , 32 );
				auto binding = SourceKeyToBinding( name );
				if ( binding.device == InputDevice::Keyboard && !binding.virtualKey )
					continue;

				for ( std::size_t slot = 0; slot < kBindingSlotCount; ++slot )
				{
					std::uintptr_t commandAddress{};
					memcpy( &commandAddress , records.data() + code * kBindingRecordSize
						+ slot * sizeof( std::uintptr_t ) , sizeof( commandAddress ) );
					if ( commandAddress < 0x10000 )
						continue;

					const std::string command = SafeReadString( commandAddress , 192 );
					const char* tokens[ 6 ] = { "+forward" , "+speed" , "+duck" , "+jump" , "+attack" , "+attack2" };
					for ( std::size_t a = 0; a < 6; ++a )
					{
						if ( !ContainsCommand( command , tokens[ a ] ) )
							continue;
						auto& candidates = refreshed[ a ];
						const bool dup = std::any_of( candidates.begin() , candidates.end() ,
							[ &binding ]( const InputBinding& c )
							{
								return c.device == binding.device && c.virtualKey == binding.virtualKey;
							} );
						if ( !dup )
							candidates.push_back( binding );
					}
				}
			}
			bindings = std::move( refreshed );
			DEV_LOG( "[helper] attack cand=%zu" , bindings[ 4 ].size() );
			for ( const auto& c : bindings[ 4 ] )
				DEV_LOG( "[helper]   atk dev=%d vk=%d" , (int)c.device , c.virtualKey );
			DEV_LOG( "[helper] attack2 cand=%zu" , bindings[ 5 ].size() );
			for ( const auto& c : bindings[ 5 ] )
				DEV_LOG( "[helper]   atk2 dev=%d vk=%d" , (int)c.device , c.virtualKey );
		}

		InputBinding Resolve( InputAction action )
		{
			RefreshLocked( std::chrono::steady_clock::now() );
			const std::size_t index = ActionIndex( action );
			if ( index >= bindings.size() )
				return {};
			const auto& candidates = bindings[ index ];

			const auto expectedMouse = action == InputAction::Attack
				? InputDevice::MousePrimary
				: action == InputAction::Attack2 ? InputDevice::MouseSecondary : InputDevice::Keyboard;
			if ( expectedMouse != InputDevice::Keyboard )
			{
				const auto expected = std::find_if( candidates.begin() , candidates.end() ,
					[ expectedMouse ]( const InputBinding& c )
					{
						return c.device == expectedMouse;
					} );
				if ( expected != candidates.end() )
					return *expected;
			}

			const auto ordinary = std::find_if( candidates.begin() , candidates.end() ,
				[]( const InputBinding& c )
				{
					return c.device != InputDevice::Keyboard
						|| ( c.virtualKey != VK_F23 && c.virtualKey != VK_F24 );
				} );
			return ordinary != candidates.end() ? *ordinary
				: candidates.empty() ? InputBinding{} : candidates.front();
		}
	};

	inline LiveInputBindings& InputBindings()
	{
		static LiveInputBindings value{};
		return value;
	}

}

static CHelper g_CHelper{};

// ============================================================================
// 视角来源(对齐项目:CreateMove 钩子传 CCSGOInput,用 CCSGOInput_GetViewAngles)
// ============================================================================
void CHelper::OnCreateMove( CCSGOInput* pInput )
{
	m_pInput = pInput;
}

bool CHelper::GetRenderCameraAngles( QAngle& out ) const
{
	if ( !m_pInput )
		return false;

	const QAngle* angles = CCSGOInput_GetViewAngles( m_pInput , 0 );
	if ( !angles )
		return false;

	out = *angles;
	return std::isfinite( out.m_x ) && std::isfinite( out.m_y ) && std::isfinite( out.m_z );
}

// ============================================================================
// 选择逻辑
// ============================================================================
std::uint8_t CHelper::ResolveWeaponKind() const
{
	auto* weapon = GetCL_Weapons()->GetLocalActiveWeapon();
	if ( !weapon )
		return 0xff;
	auto* attr = weapon->m_AttributeManager();
	if ( !attr )
		return 0xff;
	auto* item = attr->m_Item();
	if ( !item )
		return 0xff;

	const std::string name = WeaponDefinitionName( item->m_iItemDefinitionIndex() );
	if ( name == "weapon_smokegrenade" ) return static_cast<std::uint8_t>( nd::kind::smoke );
	if ( name == "weapon_flashbang" ) return static_cast<std::uint8_t>( nd::kind::flash );
	if ( name == "weapon_molotov" || name == "weapon_incgrenade" ) return static_cast<std::uint8_t>( nd::kind::molotov );
	if ( name == "weapon_hegrenade" ) return static_cast<std::uint8_t>( nd::kind::he );
	if ( name == "weapon_decoy" ) return static_cast<std::uint8_t>( nd::kind::decoy );
	return 0xff;
}

bool CHelper::Collect( const Vector3& playerPos , std::uint8_t weaponKind , std::vector<LineupView>& out ) const
{
	out.clear();
	if ( weaponKind == 0xff )
		return false;

	auto* pEngine = SDK::Interfaces::EngineToClient();
	if ( !pEngine || !pEngine->IsInGame() )
		return false;
	const char* mapRaw = pEngine->GetLevelNameShort();
	if ( !mapRaw )
		mapRaw = pEngine->GetLevelName();
	if ( !mapRaw )
		return false;

	const auto* map = FindMap( NormalizeMapName( mapRaw ) );
	if ( !map )
		return false;

	const int drawDistance = menu_state::drawDistance;
	const float maxDistanceSqr = static_cast<float>( drawDistance ) * drawDistance;

	for ( std::uint32_t i = 0; i < map->count; ++i )
	{
		const auto& entry = map->data[ i ];
		if ( static_cast<std::uint8_t>( entry.type ) != weaponKind )
			continue;

		const Vector3 position{ entry.x , entry.y , entry.z };
		const float distanceSqr = ( position - playerPos ).LengthSquared();
		if ( distanceSqr > maxDistanceSqr )
			continue;

		out.push_back( LineupView{
			entry.name , entry.action , position , entry.pitch , entry.yaw ,
			static_cast<std::uint8_t>( entry.type ) ,
			entry.actions , entry.run_ticks , entry.after_jump_ticks , entry.throw_strength , entry.manual ,
			std::sqrtf( distanceSqr ) ,
		} );
	}

	std::sort( out.begin() , out.end() , []( const LineupView& a , const LineupView& b )
	{
		return a.distance > b.distance;
	} );

	return !out.empty();
}

int CHelper::SelectArmed( const std::vector<LineupView>& lineups , const QAngle& viewAngles ) const
{
	int bestIndex = -1;
	float bestError = std::numeric_limits<float>::max();

	for ( std::size_t i = 0; i < lineups.size(); ++i )
	{
		const auto& lineup = lineups[ i ];
		if ( lineup.distance > menu_state::standRadius )
			continue;
		const float error = AngleError( viewAngles , lineup.pitch , lineup.yaw );
		if ( error < bestError )
		{
			bestError = error;
			bestIndex = static_cast<int>( i );
		}
	}
	return bestIndex;
}

bool CHelper::ExecutionPositionReady( const LineupView& lineup , const Vector3& playerPos ) const
{
	const float dx = lineup.position.m_x - playerPos.m_x;
	const float dy = lineup.position.m_y - playerPos.m_y;
	const float r = menu_state::releaseRadius;
	return dx * dx + dy * dy <= r * r
		&& std::abs( lineup.position.m_z - playerPos.m_z ) <= menu_state::heightTolerance;
}

// ============================================================================
// 模拟输入(系统级注入鼠标/键盘)
// ============================================================================
InputBinding CHelper::ResolveBinding( InputAction action ) const
{
	// 直接用 UI 里用户配置的按键,不再解析游戏绑定表(该解析在部分版本偏移错位)
	int vk = 0;
	switch ( action )
	{
	case InputAction::Forward:  vk = helper::g_move_forward.key; break;
	case InputAction::Back:     vk = helper::g_move_back.key;    break;
	case InputAction::Left:     vk = helper::g_move_left.key;    break;
	case InputAction::Right:    vk = helper::g_move_right.key;   break;
	case InputAction::Walk:     vk = helper::g_move_walk.key;    break;
	case InputAction::Duck:     vk = helper::g_move_duck.key;    break;
	case InputAction::Jump:     vk = helper::g_move_jump.key;    break;
	case InputAction::Attack:   vk = helper::g_attack_key.key;   break;
	case InputAction::Attack2:  vk = helper::g_attack2_key.key;  break;
	}
	return VkToBinding( vk );
}

bool CHelper::SetControl( OwnedControl& control , bool pressed )
{
	if ( control.pressed == pressed )
		return true;

	switch ( control.binding.device )
	{
	case InputDevice::Keyboard:
		SimulateKey( control.binding.virtualKey , pressed );
		break;
	case InputDevice::MousePrimary:
		SimulateMouseButton( MOUSEEVENTF_LEFTDOWN , MOUSEEVENTF_LEFTUP , pressed );
		break;
	case InputDevice::MouseSecondary:
		SimulateMouseButton( MOUSEEVENTF_RIGHTDOWN , MOUSEEVENTF_RIGHTUP , pressed );
		break;
	case InputDevice::MouseMiddle:
		SimulateMouseButton( MOUSEEVENTF_MIDDLEDOWN , MOUSEEVENTF_MIDDLEUP , pressed );
		break;
	default:
		return false;
	}

	control.pressed = pressed;
	return true;
}

void CHelper::ReleaseMovement( bool includeJump )
{
	if ( includeJump ) (void)SetControl( m_Jump , false );
	(void)SetControl( m_Forward , false );
	(void)SetControl( m_Walk , false );
	(void)SetControl( m_Duck , false );
}

void CHelper::ReleaseAttacks()
{
	(void)SetControl( m_Attack , false );
	(void)SetControl( m_Attack2 , false );
}

void CHelper::ResetLock()
{
	m_LockStarted = {};
	m_LockName = nullptr;
	m_LockPosition = {};
	m_LockPitch = 0.f;
	m_LockYaw = 0.f;
}

void CHelper::CancelThrow( bool latch )
{
	ReleaseAttacks();
	ReleaseMovement( true );
	m_Forward = {};
	m_Walk = {};
	m_Duck = {};
	m_Jump = {};
	m_Attack = {};
	m_Attack2 = {};
	m_ActiveLineup = {};
	m_ActivePawn = 0;
	m_ActiveWeapon = 0;
	m_ThrowPhase = ThrowPhase::Idle;
	m_PhaseTick = 0;
	m_RunStartTick = 0;
	m_JumpTick = 0;
	m_AimErrorX = m_AimErrorY = 0.f;
	m_LastAimUpdate = {};
	ResetLock();
	m_ActivationLatched = latch;
}

// ============================================================================
// 瞄准(注入相对鼠标移动)
// ============================================================================
void CHelper::AimAt( const LineupView& lineup , const QAngle& viewAngles , float& outError )
{
	constexpr float kMousemoveYaw{ 0.022f };
	// 内部读取 sensitivity cvar(每 500ms 更新一次,避免每帧遍历 cvar 列表)
	static float s_cachedSensitivity = 2.5f;
	static auto s_lastUpdate = std::chrono::steady_clock::time_point{};
	const auto aimNow = Now();
	if ( aimNow - s_lastUpdate >= std::chrono::milliseconds( 500 ) )
	{
		s_lastUpdate = aimNow;
		if ( auto* pCvar = SDK::Interfaces::EngineCvar() )
		{
			if ( auto* convar = pCvar->Find( "sensitivity" ) )
			{
				if ( convar->nType == EConVarType_Float32 )
					s_cachedSensitivity = convar->value.fl;
			}
		}
	}
	const float sensitivity = s_cachedSensitivity;

	float fovAdjust = 1.f;
	if ( auto* player = GetCL_Players()->GetLocalPlayerPawn() )
		fovAdjust = player->m_flFOVSensitivityAdjust();

	const float degPerPixel = sensitivity * kMousemoveYaw * fovAdjust;
	if ( degPerPixel <= 0.f )
		return;

	float deltaX = lineup.pitch - viewAngles.m_x;
	float deltaY = WrapYaw( lineup.yaw - viewAngles.m_y );

	outError = std::sqrtf( deltaX * deltaX + deltaY * deltaY );

	if ( menu_state::aimSpeed > 1 )
	{
		const auto now = Now();
		const float dt = m_LastAimUpdate == std::chrono::steady_clock::time_point{}
			? 0.015f
			: std::clamp( std::chrono::duration<float>( now - m_LastAimUpdate ).count() , 0.0005f , 0.05f );
		const float responseSeconds = ( 12.0f + static_cast<float>( menu_state::aimSpeed - 1 ) * 8.0f ) * 0.001f;
		const float amount = 1.0f - std::exp( -dt / responseSeconds );
		deltaX *= amount;
		deltaY *= amount;
	}
	m_LastAimUpdate = Now();

	m_AimErrorX += -deltaY / degPerPixel;
	m_AimErrorY += deltaX / degPerPixel;

	const int dx = static_cast<int>( m_AimErrorX );
	const int dy = static_cast<int>( m_AimErrorY );
	m_AimErrorX -= static_cast<float>( dx );
	m_AimErrorY -= static_cast<float>( dy );

	if ( dx != 0 || dy != 0 )
		InjectMouse( dx , dy , MOUSEEVENTF_MOVE );
}

// ============================================================================
// 投掷状态机
// ============================================================================
bool CHelper::BeginThrow( const LineupView& lineup , std::uintptr_t pawn , std::uintptr_t weapon , std::uint32_t tick , std::chrono::steady_clock::time_point now )
{
	CancelThrow( false );
	m_ActiveLineup = lineup;
	m_ActivePawn = pawn;
	m_ActiveWeapon = weapon;
	m_PhaseTick = tick;
	m_PhaseStarted = now;

	if ( lineup.actions & nd::action_run ) m_Forward.binding = ResolveBinding( InputAction::Forward );
	if ( lineup.actions & nd::action_walk ) m_Walk.binding = ResolveBinding( InputAction::Walk );
	if ( lineup.actions & nd::action_crouch ) m_Duck.binding = ResolveBinding( InputAction::Duck );
	if ( lineup.actions & nd::action_jump ) m_Jump.binding = ResolveBinding( InputAction::Jump );

	if ( lineup.throw_strength >= 0.75f )
		m_Attack.binding = ResolveBinding( InputAction::Attack );
	else if ( lineup.throw_strength <= 0.25f )
		m_Attack2.binding = ResolveBinding( InputAction::Attack2 );
	else
	{
		m_Attack.binding = ResolveBinding( InputAction::Attack );
		m_Attack2.binding = ResolveBinding( InputAction::Attack2 );
	}

	const bool missing = ( ( lineup.actions & nd::action_run ) && !m_Forward.binding )
		|| ( ( lineup.actions & nd::action_walk ) && !m_Walk.binding )
		|| ( ( lineup.actions & nd::action_crouch ) && !m_Duck.binding )
		|| ( ( lineup.actions & nd::action_jump ) && !m_Jump.binding )
		|| ( lineup.throw_strength >= 0.75f && !m_Attack.binding )
		|| ( lineup.throw_strength <= 0.25f && !m_Attack2.binding )
		|| ( lineup.throw_strength > 0.25f && lineup.throw_strength < 0.75f
			&& ( !m_Attack.binding || !m_Attack2.binding ) );
	if ( missing )
	{
		DEV_LOG( "[helper] begin_throw missing bindings (actions=%u str=%.2f fwd=%d/%d walk=%d/%d duck=%d/%d jump=%d/%d atk=%d atk2=%d)" ,
			lineup.actions , lineup.throw_strength ,
			m_Forward.binding.virtualKey , (int)m_Forward.binding.device ,
			m_Walk.binding.virtualKey , (int)m_Walk.binding.device ,
			m_Duck.binding.virtualKey , (int)m_Duck.binding.device ,
			m_Jump.binding.virtualKey , (int)m_Jump.binding.device ,
			(int)m_Attack.binding.device , (int)m_Attack2.binding.device );
		CancelThrow( true );
		return false;
	}

	if ( lineup.actions & nd::action_crouch )
	{
		if ( !SetControl( m_Duck , true ) )
		{
			DEV_LOG( "[helper] begin_throw: duck set_control failed" );
			CancelThrow( true );
			return false;
		}
		m_ThrowPhase = ThrowPhase::Crouching;
		return true;
	}

	if ( !PrimeThrow( tick , now ) )
	{
		DEV_LOG( "[helper] begin_throw: prime_throw failed" );
		CancelThrow( true );
		return false;
	}
	return true;
}

bool CHelper::PrimeThrow( std::uint32_t tick , std::chrono::steady_clock::time_point now )
{
	const float strength = m_ActiveLineup.throw_strength;
	if ( strength >= 0.75f )
	{
		if ( !SetControl( m_Attack , true ) )
		{
			DEV_LOG( "[helper] prime: attack LMB failed" );
			return false;
		}
		DEV_LOG( "[helper] prime: LMB down (str=%.2f)" , strength );
	}
	else if ( strength <= 0.25f )
	{
		if ( !SetControl( m_Attack2 , true ) )
		{
			DEV_LOG( "[helper] prime: attack2 RMB failed" );
			return false;
		}
		DEV_LOG( "[helper] prime: RMB down (str=%.2f)" , strength );
	}
	else if ( m_Attack.binding.device == InputDevice::MousePrimary
		&& m_Attack2.binding.device == InputDevice::MouseSecondary )
	{
		SimulateMouseButton( MOUSEEVENTF_LEFTDOWN , MOUSEEVENTF_LEFTUP , true );
		SimulateMouseButton( MOUSEEVENTF_RIGHTDOWN , MOUSEEVENTF_RIGHTUP , true );
		m_Attack.pressed = true;
		m_Attack2.pressed = true;
		DEV_LOG( "[helper] prime: LMB+RMB down (str=%.2f)" , strength );
	}
	else if ( !SetControl( m_Attack , true ) || !SetControl( m_Attack2 , true ) )
	{
		DEV_LOG( "[helper] prime: dual set_control failed" );
		return false;
	}

	m_ThrowPhase = ThrowPhase::Priming;
	m_PhaseTick = tick;
	m_PhaseStarted = now;
	return true;
}

void CHelper::FinishThrow( std::uint32_t tick )
{
	const bool releaseAfter = ( m_ActiveLineup.actions & nd::action_release_movement_after_throw ) != 0;
	if ( releaseAfter )
	{
		ReleaseAttacks();
		ReleaseMovement( false );
	}
	else
	{
		ReleaseMovement( false );
		ReleaseAttacks();
	}

	m_ActivationLatched = true;
	m_PhaseTick = tick;
	if ( m_ActiveLineup.actions & nd::action_jump )
		m_ThrowPhase = ThrowPhase::Complete;
	else
		CancelThrow( true );
}

void CHelper::DriveThrow( std::uintptr_t pawn , std::uintptr_t weapon , std::uint32_t tick , std::chrono::steady_clock::time_point now )
{
	switch ( m_ThrowPhase )
	{
	case ThrowPhase::Crouching:
	{
		float duckAmount = 0.f;
		bool ducked = false;
		if ( auto* mv = reinterpret_cast<C_CSPlayerPawn*>( pawn )->m_pMovementServices() )
		{
			auto* ms = reinterpret_cast<CCSPlayer_MovementServices*>( mv );
			duckAmount = ms->m_flDuckAmount();
			ducked = ms->m_bDucked();
		}
		if ( ducked || duckAmount >= 0.94f )
		{
			if ( !PrimeThrow( tick , now ) )
				CancelThrow( true );
		}
		else if ( now - m_PhaseStarted > std::chrono::milliseconds( 900 ) )
		{
			DEV_LOG( "[helper] crouch timeout: duckAmount=%.2f ducked=%d" , duckAmount , ducked ? 1 : 0 );
			CancelThrow( true );
		}
		return;
	}
	case ThrowPhase::Priming:
	{
		auto* grenade = reinterpret_cast<C_BaseCSGrenade*>( weapon );
		bool pinPulled = false;
		float strength = -1.f;
		if ( grenade )
		{
			pinPulled = grenade->m_bPinPulled();
			strength = grenade->m_flThrowStrength();
		}

		const float target = m_ActiveLineup.throw_strength;
		const bool ready = pinPulled && std::isfinite( strength )
			&& std::abs( strength - target ) <= 0.08f
			&& tick != m_PhaseTick;
		if ( !ready )
		{
			if ( now - m_PhaseStarted > std::chrono::milliseconds( 3000 ) )
			{
				DEV_LOG( "[helper] priming timeout: pin=%d str=%.3f target=%.3f phaseTick=%u tick=%u" ,
					pinPulled ? 1 : 0 , strength , target , m_PhaseTick , tick );
				CancelThrow( true );
			}
			return;
		}

		if ( m_ActiveLineup.actions & nd::action_run )
		{
			if ( !m_Forward.pressed )
			{
				if ( ( m_ActiveLineup.actions & nd::action_walk ) && !SetControl( m_Walk , true ) )
				{
					CancelThrow( true );
					return;
				}
				if ( !SetControl( m_Forward , true ) )
				{
					CancelThrow( true );
					return;
				}
				m_RunStartTick = tick;
			}
			m_ThrowPhase = ThrowPhase::Running;
			if ( tick - m_RunStartTick < m_ActiveLineup.run_ticks )
				return;
		}
		else if ( !( m_ActiveLineup.actions & nd::action_jump ) )
		{
			FinishThrow( tick );
			return;
		}
		[[fallthrough]];
	}
	case ThrowPhase::Running:
		if ( tick - m_RunStartTick < m_ActiveLineup.run_ticks )
			return;
		if ( m_ActiveLineup.actions & nd::action_jump )
		{
			if ( !SetControl( m_Jump , true ) )
			{
				CancelThrow( true );
				return;
			}
			m_JumpTick = tick;
			m_PhaseStarted = now;
			m_ThrowPhase = ThrowPhase::Jumping;
		}
		else
			FinishThrow( tick );
		return;
	case ThrowPhase::Jumping:
	{
		const std::uint32_t elapsedTicks = tick - m_JumpTick;
		const auto afterJump = m_ActiveLineup.after_jump_ticks;
		if ( elapsedTicks < afterJump )
			return;

		if ( afterJump == 0 )
		{
			if ( now > m_PhaseStarted )
				FinishThrow( tick );
			return;
		}

		auto* player = reinterpret_cast<C_CSPlayerPawn*>( pawn );
		const std::uint32_t flags = player->m_fFlags();
		const Vector3 velocity = player->m_vecAbsVelocity();
		const bool airborne = ( flags & 1u ) == 0u
			|| ( std::isfinite( velocity.m_z ) && std::abs( velocity.m_z ) > 12.f );
		if ( airborne )
		{
			FinishThrow( tick );
			return;
		}

		if ( now - m_PhaseStarted > std::chrono::milliseconds( 250 ) )
			CancelThrow( true );
		return;
	}
	case ThrowPhase::Complete:
		if ( tick != m_PhaseTick )
			CancelThrow( true );
		return;
	default:
		return;
	}
}

// ============================================================================
// 主循环
// ============================================================================
void CHelper::Tick()
{
	if ( !menu_state::helperEnabled )
	{
		const bool ownsInput = m_ThrowPhase != ThrowPhase::Idle
			|| m_Forward.pressed || m_Walk.pressed || m_Duck.pressed
			|| m_Jump.pressed || m_Attack.pressed || m_Attack2.pressed;
		if ( ownsInput )
			CancelThrow( false );
		m_ActivationLatched = false;
		ResetLock();
		return;
	}

	const bool activationHeld = helper::g_helper_key.active();
	if ( !activationHeld )
	{
		const bool ownsInput = m_ThrowPhase != ThrowPhase::Idle
			|| m_Forward.pressed || m_Walk.pressed || m_Duck.pressed
			|| m_Jump.pressed || m_Attack.pressed || m_Attack2.pressed;
		if ( ownsInput )
			CancelThrow( false );
		m_ActivationLatched = false;
		m_AimErrorX = m_AimErrorY = 0.f;
		ResetLock();
		return;
	}

	auto* player = GetCL_Players()->GetLocalPlayerPawn();
	auto* weapon = GetCL_Weapons()->GetLocalActiveWeapon();
	if ( !player || !player->IsAlive() || !weapon || !GameHasInputFocus() || player->m_bIsBuyMenuOpen() )
	{
		CancelThrow( activationHeld );
		m_AimErrorX = m_AimErrorY = 0.f;
		return;
	}

	// 回合冻结/回合介绍/暂停时不投掷
	if ( void* rules = SDK::Pointers::GameRules() )
	{
		auto* csRules = reinterpret_cast<C_CSGameRules*>( rules );
		if ( csRules->m_bFreezePeriod() || csRules->m_bTeamIntroPeriod() || csRules->m_bGamePaused() )
		{
			CancelThrow( activationHeld );
			m_AimErrorX = m_AimErrorY = 0.f;
			return;
		}
	}

	const std::uint8_t kind = ResolveWeaponKind();
	if ( kind == 0xff )
	{
		CancelThrow( activationHeld );
		return;
	}

	const std::uint32_t tick = TickCount();

	if ( m_ThrowPhase != ThrowPhase::Idle )
	{
		if ( menu_state::autoAim )
		{
			QAngle liveView;
			if ( GetRenderCameraAngles( liveView ) )
			{
				float error = 0.f;
				AimAt( m_ActiveLineup , liveView , error );
			}
		}
		DriveThrow( reinterpret_cast<std::uintptr_t>( player ) , reinterpret_cast<std::uintptr_t>( weapon ) , tick , Now() );
		return;
	}

	if ( m_ActivationLatched )
		return;

	const Vector3 playerPos = player->GetOrigin();
	if ( !Collect( playerPos , kind , m_TickScratch ) )
	{
		ResetLock();
		return;
	}

	QAngle viewAngles;
	if ( !GetRenderCameraAngles( viewAngles ) )
	{
		m_AimErrorX = m_AimErrorY = 0.f;
		return;
	}
	const int index = SelectArmed( m_TickScratch , viewAngles );
	if ( index < 0 )
	{
		m_AimErrorX = m_AimErrorY = 0.f;
		ResetLock();
		return;
	}

	const auto& lineup = m_TickScratch[ static_cast<std::size_t>( index ) ];
	float error = AngleError( viewAngles , lineup.pitch , lineup.yaw );
	if ( menu_state::autoAim )
		AimAt( lineup , viewAngles , error );

	const Vector3 velocity = player->m_vecAbsVelocity();
	const bool stationary = std::isfinite( velocity.m_x ) && std::isfinite( velocity.m_y ) && std::isfinite( velocity.m_z )
		&& ( velocity.m_x * velocity.m_x + velocity.m_y * velocity.m_y ) <= 144.f
		&& std::abs( velocity.m_z ) <= 12.f;

	const bool positionReady = ExecutionPositionReady( lineup , playerPos );
	const bool lockMatches = m_LockName == lineup.name
		&& ( m_LockPosition - lineup.position ).LengthSquared() <= 0.01f
		&& std::abs( m_LockPitch - lineup.pitch ) <= 0.001f
		&& std::abs( WrapYaw( m_LockYaw - lineup.yaw ) ) <= 0.001f;

	if ( positionReady && stationary && error <= menu_state::aimThreshold )
	{
		if ( !lockMatches )
		{
			m_LockName = lineup.name;
			m_LockPosition = lineup.position;
			m_LockPitch = lineup.pitch;
			m_LockYaw = lineup.yaw;
			m_LockStarted = Now();
		}
	}
	else
	{
		ResetLock();
	}

	const bool settled = m_LockStarted != std::chrono::steady_clock::time_point{}
		&& Now() - m_LockStarted
			>= std::chrono::milliseconds( std::clamp( menu_state::lockTimeMs , 0 , 250 ) );

	if ( menu_state::autoExecute && !lineup.manual && settled )
	{
		if ( !BeginThrow( lineup , reinterpret_cast<std::uintptr_t>( player ) ,
			reinterpret_cast<std::uintptr_t>( weapon ) , tick , Now() ) )
			m_ActivationLatched = true;
	}
}

// ============================================================================
// 渲染
// ============================================================================
namespace
{
	// esp_icons 投掷物图标字符
	const char* HelperKindIcon( std::uint8_t kind )
	{
		switch ( static_cast<nd::kind>( kind ) )
		{
		case nd::kind::flash:   return "\x63";  // 闪光弹
		case nd::kind::he:      return "\x64";  // 手雷
		case nd::kind::smoke:   return "\x65";  // 烟雾弹
		case nd::kind::molotov: return "\x66";  // 燃烧弹
		case nd::kind::decoy:   return "\x26";  // 诱饵弹
		default:                return nullptr;
		}
	}

	ImU32 AccentColor( float alpha )
	{
		return IM_COL32(
			static_cast<int>( vars::colorAccent[ 0 ] * 255.f ) ,
			static_cast<int>( vars::colorAccent[ 1 ] * 255.f ) ,
			static_cast<int>( vars::colorAccent[ 2 ] * 255.f ) ,
			static_cast<int>( alpha ) );
	}
}

void CHelper::DrawMarker( ImDrawList* drawList , const LineupView& lineup , bool selected , float yOffset ) const
{
	ImVec2 screen;
	const Vector3 world = lineup.position + Vector3{ 0.f , 0.f , 8.f };
	if ( !Math::WorldToScreen( world , screen ) )
		return;

	ImFont* font = ImGui::GetFont();
	const float fontSize = font ? font->FontSize : 15.f;

	const std::string title = lineup.name ? lineup.name : "?";
	const ImVec2 titleSize = font
		? font->CalcTextSizeA( fontSize , FLT_MAX , -1.f , title.c_str() )
		: ImVec2( 100.f , 20.f );

	// 副标题:投掷动作 + 距离(米)
	std::string subtitle;
	if ( menu_state::showAction && lineup.action )
		subtitle = lineup.action;
	if ( menu_state::showDistance )
	{
		if ( !subtitle.empty() )
			subtitle += "  ";
		char buf[ 32 ];
		std::snprintf( buf , sizeof( buf ) , "%.0fm" , lineup.distance / 52.0f );
		subtitle += buf;
	}
	const ImVec2 subSize = ( !subtitle.empty() && font )
		? font->CalcTextSizeA( fontSize - 2.f , FLT_MAX , -1.f , subtitle.c_str() )
		: ImVec2{};

	const float textW = std::max( titleSize.x , subSize.x );
	const float textH = titleSize.y + ( subSize.y > 0.f ? 2.f + subSize.y : 0.f );

	const float alpha = std::clamp( 255.f - lineup.distance * 0.12f , 75.f , 255.f );
	const ImU32 base = AccentColor( alpha );

	// 布局(Hysteria 风格:图标槽 + 分隔线 + 文本)
	const float iconSize = 18.f;
	const float padX = 6.f;
	const float padY = 4.f;
	const float cardW = iconSize + 6.f + textW + 14.f;
	const float cardH = std::max( iconSize , textH ) + padY * 2.f;

	const float left = screen.x - cardW * 0.5f;
	const float top = screen.y - cardH - 18.f + yOffset;
	const float iconLeft = left + padX;
	const float iconTop = top + ( cardH - iconSize ) * 0.5f;
	const float dividerX = iconLeft + iconSize + 3.f;
	const float textLeft = dividerX + 6.f;
	const float textTop = top + ( cardH - textH ) * 0.5f;

	// 背景卡片(半透明黑,圆角)
	const ImU32 bg = IM_COL32( 0 , 0 , 0 , static_cast<int>( alpha * 0.72f ) );
	drawList->AddRectFilled( { left , top } , { left + cardW , top + cardH } , bg , 4.f );

	// 投掷物图标(esp_icons)
	const char* icon = HelperKindIcon( lineup.kind );
	if ( icon && g_font && g_font->f_weapon_icons.get_font() )
	{
		ImFont* iconFont = g_font->f_weapon_icons.get_font();
		const float iconFontSize = 14.f;
		const ImVec2 iconTextSize = iconFont->CalcTextSizeA( iconFontSize , FLT_MAX , -1.f , icon );
		drawList->AddText( iconFont , iconFontSize ,
			{ iconLeft + ( iconSize - iconTextSize.x ) * 0.5f , iconTop + ( iconSize - iconTextSize.y ) * 0.5f } ,
			base , icon );
	}
	else
	{
		drawList->AddRectFilled( { iconLeft , iconTop } , { iconLeft + iconSize , iconTop + iconSize } , base , 2.f );
	}

	// 分隔线
	drawList->AddRectFilled( { dividerX , top + 2.f } , { dividerX + 2.f , top + cardH - 2.f } , base , 1.f );

	// 文本(标题 + 副标题:动作/距离)
	if ( font )
	{
		drawList->AddText( font , fontSize , { textLeft , textTop } , IM_COL32( 245 , 247 , 252 , static_cast<int>( alpha ) ) , title.c_str() );
		if ( !subtitle.empty() )
			drawList->AddText( font , fontSize - 2.f , { textLeft , textTop + titleSize.y + 2.f } , base , subtitle.c_str() );
	}
}

void CHelper::DrawStandMarker( ImDrawList* drawList , const LineupView& lineup , bool standing ) const
{
	const ImU32 color = standing ? IM_COL32( 110 , 235 , 140 , 245 ) : IM_COL32( 120 , 170 , 255 , 235 );

	constexpr int segments{ 28 };
	std::vector<ImVec2> points;
	points.reserve( segments );

	for ( int i = 0; i < segments; ++i )
	{
		const float theta = ( static_cast<float>( i ) / segments ) * 2.0f * std::numbers::pi_v<float>;
		const Vector3 world{
			lineup.position.m_x + std::cosf( theta ) * menu_state::standRadius ,
			lineup.position.m_y + std::sinf( theta ) * menu_state::standRadius ,
			lineup.position.m_z + 1.0f ,
		};
		ImVec2 screen;
		if ( !Math::WorldToScreen( world , screen ) )
			return;
		points.push_back( screen );
	}

	if ( points.size() > 1 )
		drawList->AddPolyline( points.data() , static_cast<int>( points.size() ) , color , true , standing ? 2.0f : 1.4f );

	ImVec2 center;
	if ( Math::WorldToScreen( lineup.position + Vector3{ 0.f , 0.f , 1.f } , center ) )
		drawList->AddCircleFilled( center , standing ? 2.5f : 1.8f , color , 10 );
}

// 描点(背景圈+瞄点圈+名称/action)
void CHelper::DrawMouseAimPoints( ImDrawList* drawList , int screenW , int screenH ) const
{
	if ( m_RenderScratch.empty() )
		return;

	QAngle viewAngles;
	if ( !GetRenderCameraAngles( viewAngles ) )
		return;

	const Vector3 eyePos = GetCL_Players()->GetLocalEyeOrigin();
	const Vector3 playerPos = GetCL_Players()->GetLocalPlayerPawn()->GetOrigin();

	const ImU32 themeCol = AccentColor( 255 );
	const ImU32 grayCol = IM_COL32( 180 , 180 , 180 , 200 );
	const ImU32 grayTextCol = IM_COL32( 220 , 220 , 220 , 200 );

	// 第一遍:收集瞄点屏幕位置 + 就绪状态
	struct AimScreen
	{
		const LineupView* lineup;
		ImVec2 screen;
		bool converged;
	};
	std::vector<AimScreen> aims;

	for ( std::size_t i = 0; i < m_RenderScratch.size(); ++i )
	{
		const auto& lineup = m_RenderScratch[ i ];
		if ( lineup.distance > menu_state::standRadius )
			continue;

		// 就绪 = 精确站位(release_radius 内) + 瞄准到位(误差 <= aim_threshold)
		const bool converged = ExecutionPositionReady( lineup , playerPos )
			&& AngleError( viewAngles , lineup.pitch , lineup.yaw ) <= menu_state::aimThreshold;

		Vector3 forward;
		const QAngle angles{ lineup.pitch , lineup.yaw , 0.f };
		Math::AngleVectors( angles , forward );

		// 从玩家眼睛沿投掷方向 220u(跟准星方向一致)
		const Vector3 worldAim = eyePos + forward * 220.f;

		ImVec2 screen;
		if ( !Math::WorldToScreen( worldAim , screen ) )
			continue;
		if ( screen.x < 0.f || screen.x > (float)screenW || screen.y < 0.f || screen.y > (float)screenH )
			continue;

		aims.push_back( { &lineup , screen , converged } );
	}

	// 第二遍:绘制
	for ( const auto& a : aims )
	{
		const bool converged = a.converged;

		// 背景圈 + 瞄点圈(就绪主题色,未就绪灰色)
		drawList->AddCircleFilled( a.screen , 8.f , IM_COL32( 0 , 0 , 0 , 64 ) , 16 );
		const ImU32 ring = converged ? themeCol : grayCol;
		drawList->AddCircle( a.screen , 6.f , ring , 16 , converged ? 2.f : 1.f );

		// 描点卡片(黑底圆角 + 名称/action,就绪主题色/未就绪灰色)
		ImFont* font = ImGui::GetFont();
		const float fontSize = font ? font->FontSize : 14.f;
		if ( font )
		{
			const std::string name = a.lineup->name ? a.lineup->name : "?";
			std::string action;
			if ( menu_state::showAction && a.lineup->action )
				action = a.lineup->action;
			const ImU32 textCol = converged ? themeCol : grayTextCol;

			const ImVec2 nameSize = font->CalcTextSizeA( fontSize , FLT_MAX , -1.f , name.c_str() );
			const ImVec2 actionSize = !action.empty()
				? font->CalcTextSizeA( fontSize - 2.f , FLT_MAX , -1.f , action.c_str() )
				: ImVec2{};

			const float textW = std::max( nameSize.x , actionSize.x );
			const float textH = nameSize.y + ( actionSize.y > 0.f ? 2.f + actionSize.y : 0.f );
			const float padX = 5.f;
			const float padY = 3.f;
			const ImVec2 boxPos( a.screen.x + 14.f - padX , a.screen.y - 9.f - padY );
			const ImVec2 boxEnd( boxPos.x + textW + padX * 2.f , boxPos.y + textH + padY * 2.f );
			drawList->AddRectFilled( boxPos , boxEnd , IM_COL32( 0 , 0 , 0 , 150 ) , 3.f );

			float ty = boxPos.y + padY;
			drawList->AddText( font , fontSize , { boxPos.x + padX , ty } , textCol , name.c_str() );
			ty += nameSize.y + 2.f;
			if ( !action.empty() )
				drawList->AddText( font , fontSize - 2.f , { boxPos.x + padX , ty } , textCol , action.c_str() );
		}
	}
}

// 渲染主入口
auto CHelper::OnRender( ImDrawList* drawList , int screenW , int screenH ) -> void
{
	if ( !drawList )
		return;

	Tick();

	auto* player = GetCL_Players()->GetLocalPlayerPawn();
	if ( !player || !player->IsAlive() || !menu_state::helperEnabled )
		return;

	const Vector3 playerPos = player->GetOrigin();
	const std::uint8_t kind = ResolveWeaponKind();
	if ( !Collect( playerPos , kind , m_RenderScratch ) )
		return;

	// 点位名牌(Lua 方式:同投掷物 + 位置 20u 内 = 同一个站位,合并成一个名牌,名字垂直堆叠)
	struct Group
	{
		std::uint8_t kind;
		Vector3 anchor;
		std::vector<const LineupView*> points;
	};
	std::vector<Group> groups;
	for ( const auto& point : m_RenderScratch )
	{
		Group* found = nullptr;
		for ( auto& g : groups )
		{
			if ( g.kind == point.kind && ( g.anchor - point.position ).Length() <= 20.f )
			{
				found = &g;
				break;
			}
		}
		if ( !found )
		{
			groups.push_back( { point.kind , point.position , {} } );
			found = &groups.back();
		}
		found->points.push_back( &point );
	}

	for ( const auto& g : groups )
	{
		// 组中心投影(名牌位置)
		Vector3 center;
		for ( const auto* p : g.points )
			center += p->position;
		center *= 1.f / static_cast<float>( g.points.size() );
		center.m_z += 8.f;

		ImVec2 screen;
		if ( !Math::WorldToScreen( center , screen ) )
			continue;
		if ( screen.x < 0.f || screen.x > (float)screenW || screen.y < 0.f || screen.y > (float)screenH )
			continue;

		ImFont* font = ImGui::GetFont();
		const float fontSize = font ? font->FontSize : 14.f;

		// 名牌尺寸:图标槽 + 分隔线 + 垂直堆叠的名字(带距离)
		const float iconSize = 18.f;
		const float padX = 6.f;
		const float padY = 4.f;

		float maxTextW = 0.f;
		float totalH = 0.f;
		std::vector<ImVec2> sizes;
		for ( const auto* p : g.points )
		{
			std::string name = p->name ? p->name : "?";
			if ( menu_state::showDistance )
			{
				char buf[ 32 ];
				std::snprintf( buf , sizeof( buf ) , "  %.0fm" , p->distance / 52.0f );
				name += buf;
			}
			const ImVec2 sz = font
				? font->CalcTextSizeA( fontSize , FLT_MAX , -1.f , name.c_str() )
				: ImVec2( 60.f , 16.f );
			sizes.push_back( sz );
			maxTextW = std::max( maxTextW , sz.x );
			totalH += std::max( 0.f , sz.y - 1.f );
		}

		const float cardW = iconSize + 6.f + maxTextW + padX * 2.f + 8.f;
		const float cardH = std::max( iconSize , totalH ) + padY * 2.f;

		const float left = screen.x - cardW * 0.5f;
		const float top = screen.y - cardH - 18.f;

		const ImU32 base = AccentColor( 255 );
		drawList->AddRectFilled( { left , top } , { left + cardW , top + cardH } , IM_COL32( 0 , 0 , 0 , 150 ) , 4.f );

		// 图标(投掷物)
		const float iconLeft = left + padX;
		const float iconTop = top + ( cardH - iconSize ) * 0.5f;
		const char* icon = HelperKindIcon( g.kind );
		if ( icon && g_font && g_font->f_weapon_icons.get_font() )
		{
			ImFont* iconFont = g_font->f_weapon_icons.get_font();
			const float iconFontSize = 14.f;
			const ImVec2 iconTextSize = iconFont->CalcTextSizeA( iconFontSize , FLT_MAX , -1.f , icon );
			drawList->AddText( iconFont , iconFontSize ,
				{ iconLeft + ( iconSize - iconTextSize.x ) * 0.5f , iconTop + ( iconSize - iconTextSize.y ) * 0.5f } ,
				base , icon );
		}
		else
		{
			drawList->AddRectFilled( { iconLeft , iconTop } , { iconLeft + iconSize , iconTop + iconSize } , base , 2.f );
		}

		// 分隔线
		const float dividerX = iconLeft + iconSize + 3.f;
		drawList->AddRectFilled( { dividerX , top + 2.f } , { dividerX + 2.f , top + cardH - 2.f } , base , 1.f );

		// 垂直堆叠名字
		float y = top + ( cardH - totalH ) * 0.5f;
		for ( std::size_t i = 0; i < g.points.size(); ++i )
		{
			std::string name = g.points[ i ]->name ? g.points[ i ]->name : "?";
			if ( menu_state::showDistance )
			{
				char buf[ 32 ];
				std::snprintf( buf , sizeof( buf ) , "  %.0fm" , g.points[ i ]->distance / 52.0f );
				name += buf;
			}
			drawList->AddText( font , fontSize , { dividerX + 6.f , y } , IM_COL32( 245 , 247 , 252 , 255 ) , name.c_str() );
			y += std::max( 0.f , sizes[ i ].y - 1.f );
		}
	}

	// 站圈(只显示 armed 选中的点位)
	QAngle viewAngles;
	if ( GetRenderCameraAngles( viewAngles ) )
	{
		const int armedIndex = SelectArmed( m_RenderScratch , viewAngles );
		if ( armedIndex >= 0 )
		{
			const auto& lineup = m_RenderScratch[ armedIndex ];
			const bool inZone = lineup.distance <= menu_state::standRadius;
			if ( inZone )
				DrawStandMarker( drawList , lineup , true );
		}
	}

	// 描点(当前走到的点位对应的所有描点)
	DrawMouseAimPoints( drawList , screenW , screenH );
}

auto GetHelper() -> CHelper*
{
	return &g_CHelper;
}
