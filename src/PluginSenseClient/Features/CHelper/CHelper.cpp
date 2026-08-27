#include "CHelper.hpp"
#include "CHelperRecorder.hpp"

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

	// 执行控制台命令(经 InputService,与 CNameChanger::RunCommand 同机制)
	void RunConsoleCommand( const char* cmd )
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

// ============================================================================
// 录制(手动快照)
// ============================================================================
std::string CHelper::GetCurrentMapName() const
{
	auto* pEngine = SDK::Interfaces::EngineToClient();
	if ( !pEngine || !pEngine->IsInGame() )
		return {};

	const char* mapRaw = pEngine->GetLevelNameShort();
	if ( !mapRaw )
		mapRaw = pEngine->GetLevelName();
	if ( !mapRaw )
		return {};
	return NormalizeMapName( mapRaw );
}

std::string CHelper::KindLabel( std::uint8_t kind ) const
{
	switch ( kind )
	{
	case static_cast<std::uint8_t>( nd::kind::smoke ):  return "Smoke";
	case static_cast<std::uint8_t>( nd::kind::flash ):  return "Flash";
	case static_cast<std::uint8_t>( nd::kind::molotov ): return "Molotov";
	case static_cast<std::uint8_t>( nd::kind::he ):     return "HE";
	case static_cast<std::uint8_t>( nd::kind::decoy ):  return "Decoy";
	default: return "?";
	}
}


std::vector<std::string> CHelper::BuildRecorderItems( std::uint8_t kindFilter ) const
{
	std::vector<std::string> items;
	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() )
		return items;

	if ( auto* recorder = GetHelperRecorder() )
	{
		if ( const auto* list = recorder->Get( mapName ) )
		{
			items.reserve( list->size() );
			for ( std::size_t i = 0; i < list->size(); ++i )
			{
				const auto& lu = ( *list )[ i ];
				if ( kindFilter != 0xff && lu.kind != kindFilter )
					continue;

				// 隐藏/覆盖标记放开头,与 Builtin 列表风格一致
				std::string label;
				if ( lu.hidden )
					label += "[hidden] ";
				else if ( lu.override_builtin_index >= 0 )
					label += "[override] ";
				label += std::to_string( i + 1 ) + ". " + lu.name;
				label += " (" + KindLabel( lu.kind ) + ")";
				if ( !lu.action.empty() )
					label += " " + lu.action;
				items.push_back( std::move( label ) );
			}
		}
	}
	return items;
}

bool CHelper::RemoveRecorderItem( int index )
{
	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() || index < 0 )
		return false;
	if ( auto* recorder = GetHelperRecorder() )
		return recorder->Remove( mapName , static_cast<std::size_t>( index ) );
	return false;
}

void CHelper::ClearRecorderMap()
{
	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() )
		return;
	if ( auto* recorder = GetHelperRecorder() )
		recorder->ClearMap( mapName );
}

bool CHelper::GetRecorderItem( int index , UserLineup& out ) const
{
	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() || index < 0 )
		return false;
	if ( auto* recorder = GetHelperRecorder() )
	{
		if ( const auto* list = recorder->Get( mapName ) )
		{
			if ( static_cast<std::size_t>( index ) < list->size() )
			{
				out = ( *list )[ static_cast<std::size_t>( index ) ];
				return true;
			}
		}
	}
	return false;
}

bool CHelper::UpdateRecorderItem( int index , const UserLineup& lineup )
{
	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() || index < 0 )
		return false;
	if ( auto* recorder = GetHelperRecorder() )
		return recorder->Update( mapName , static_cast<std::size_t>( index ) , lineup );
	return false;
}

int CHelper::GetRecorderIndexAt( int listPos , std::uint8_t kindFilter ) const
{
	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() || listPos < 0 )
		return -1;

	if ( auto* recorder = GetHelperRecorder() )
	{
		if ( const auto* list = recorder->Get( mapName ) )
		{
			int matched = 0;
			for ( std::size_t i = 0; i < list->size(); ++i )
			{
				const auto& lu = ( *list )[ i ];
				if ( kindFilter != 0xff && lu.kind != kindFilter )
					continue;
				if ( matched == listPos )
					return static_cast<int>( i );
				++matched;
			}
		}
	}
	return -1;
}

std::vector<std::string> CHelper::BuildBuiltinItems( std::uint8_t kindFilter ) const
{
	std::vector<std::string> items;
	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() )
		return items;

	const auto* map = FindMap( mapName );
	if ( !map )
		return items;

	// 查某内置点位是否被编辑/隐藏(返回 0=未动 1=已编辑 2=已隐藏)
	auto overrideState = [&]( std::uint32_t builtinIndex ) -> int
	{
		if ( auto* recorder = GetHelperRecorder() )
		{
			if ( const auto* list = recorder->Get( mapName ) )
			{
				for ( const auto& lu : *list )
				{
					if ( lu.override_builtin_index == static_cast<int>( builtinIndex ) )
						return lu.hidden ? 2 : 1;
				}
			}
		}
		return 0;
	};

	items.reserve( map->count );
	for ( std::uint32_t i = 0; i < map->count; ++i )
	{
		const auto& e = map->data[ i ];
		const std::uint8_t eKind = static_cast<std::uint8_t>( e.type );
		if ( kindFilter != 0xff && eKind != kindFilter )
			continue;

		std::string label = "[" + std::to_string( i ) + "]";
		// 被覆盖编辑/隐藏的内置点位,开头加标记
		const int state = overrideState( i );
		if ( state == 2 )
			label += " [hidden]";
		else if ( state == 1 )
			label += " [edited]";
		label += " " + std::string( e.name ? e.name : "" );
		label += " (" + KindLabel( eKind ) + ")";
		if ( e.action )
		{
			label += " ";
			label += e.action;
		}
		items.push_back( std::move( label ) );
	}
	return items;
}

bool CHelper::GetBuiltinItem( int listPos , std::uint8_t kindFilter , UserLineup& out ) const
{
	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() || listPos < 0 )
		return false;

	const auto* map = FindMap( mapName );
	if ( !map )
		return false;

	// 按筛选定位:跳过非匹配类型,listPos 是"筛选后列表"的位置
	int matched = 0;
	for ( std::uint32_t i = 0; i < map->count; ++i )
	{
		const auto& e = map->data[ i ];
		const std::uint8_t eKind = static_cast<std::uint8_t>( e.type );
		if ( kindFilter != 0xff && eKind != kindFilter )
			continue;

		if ( matched == listPos )
		{
			// 若该内置点位已被用户覆盖(编辑/隐藏),载入覆盖版本(含 hidden)
			if ( auto* recorder = GetHelperRecorder() )
			{
				if ( const auto* list = recorder->Get( mapName ) )
				{
					for ( const auto& lu : *list )
					{
						if ( lu.override_builtin_index == static_cast<int>( i ) )
						{
							out = lu; // 覆盖条目已含 hidden 与原始索引
							return true;
						}
					}
				}
			}

			out.name = e.name ? e.name : "";
			out.action = e.action ? e.action : "";
			out.x = e.x; out.y = e.y; out.z = e.z;
			out.pitch = e.pitch; out.yaw = e.yaw;
			out.kind = eKind;
			out.actions = e.actions;
			out.run_ticks = e.run_ticks;
			out.after_jump_ticks = e.after_jump_ticks;
			out.throw_strength = e.throw_strength;
			out.manual = e.manual;
			out.override_builtin_index = static_cast<int>( i ); // 内置原始索引
			return true;
		}
		++matched;
	}
	return false;
}

bool CHelper::SaveBuiltinOverride( int builtinIndex , const UserLineup& lineup )
{
	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() || builtinIndex < 0 )
		return false;

	auto* recorder = GetHelperRecorder();
	if ( !recorder )
		return false;

	// 取内置原值,判断 lineup 是否与内置原值完全相同
	const auto* map = FindMap( mapName );
	bool sameAsBuiltin = false;
	if ( map && static_cast<std::uint32_t>( builtinIndex ) < map->count )
	{
		const auto& e = map->data[ builtinIndex ];
		sameAsBuiltin =
			lineup.hidden == false
			&& lineup.name == ( e.name ? e.name : "" )
			&& lineup.action == ( e.action ? e.action : "" )
			&& lineup.x == e.x && lineup.y == e.y && lineup.z == e.z
			&& lineup.pitch == e.pitch && lineup.yaw == e.yaw
			&& lineup.kind == static_cast<std::uint8_t>( e.type )
			&& lineup.actions == e.actions
			&& lineup.run_ticks == e.run_ticks
			&& lineup.after_jump_ticks == e.after_jump_ticks
			&& lineup.throw_strength == e.throw_strength
			&& lineup.manual == e.manual;
	}

	// 与内置原值完全相同 → 不是一次真正的编辑:删除覆盖条目,恢复内置原样
	if ( sameAsBuiltin )
		return RemoveBuiltinOverride( builtinIndex );

	// 已有同索引覆盖条目 → 更新
	if ( const auto* list = recorder->Get( mapName ) )
	{
		for ( std::size_t i = 0; i < list->size(); ++i )
		{
			if ( ( *list )[ i ].override_builtin_index == builtinIndex )
			{
				UserLineup updated = lineup;
				updated.override_builtin_index = builtinIndex;
				return recorder->Update( mapName , i , updated );
			}
		}
	}

	// 无则新增覆盖条目
	UserLineup added = lineup;
	added.override_builtin_index = builtinIndex;
	recorder->Add( mapName , added );
	return true;
}

bool CHelper::RemoveBuiltinOverride( int builtinIndex )
{
	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() || builtinIndex < 0 )
		return false;

	auto* recorder = GetHelperRecorder();
	if ( !recorder )
		return false;

	if ( const auto* list = recorder->Get( mapName ) )
	{
		for ( std::size_t i = 0; i < list->size(); ++i )
		{
			if ( ( *list )[ i ].override_builtin_index == builtinIndex )
				return recorder->Remove( mapName , i );
		}
	}
	return false;
}

void CHelper::TeleportTo( const UserLineup& lineup )
{
	// setpos x y z; setang pitch yaw 0(roll 恒为 0,投掷点位无翻滚)
	char cmd[ 256 ];
	std::snprintf( cmd , sizeof( cmd ) , "setpos %f %f %f; setang %f %f 0" ,
		lineup.x , lineup.y , lineup.z , lineup.pitch , lineup.yaw );
	RunConsoleCommand( cmd );
	DEV_LOG( "[helper] teleport -> %s" , cmd );
}

void CHelper::SetRecordName( const std::string& name )
{
	m_RecordName = name;
}

// ============================================================================
// 录制会话(toggle 键:按下开始,再按一下保存)
// 会话中跟踪:拔销站位、起跳时机、跑动 tick、蹲姿、出手力度与视角
// ============================================================================
void CHelper::BeginRecordSession()
{
	m_RecordSessionActive = true;
	m_SessionReady = false;
	m_SessionPinArmed = false;
	m_SessionWasAirborne = false;
	m_SessionWasCrouch = false;
	m_SessionWasWalk = false;
	m_SessionThrew = false;
	m_SessionArmTick = 0;
	m_SessionJumpTick = 0;
	m_SessionThrowTick = 0;
	m_SessionRunTicks = 0;
	m_SessionAfterJumpTicks = 0;
	m_SessionArmPos = {};
	m_SessionThrowAngles = {};
	m_SessionStrength = 1.f;
	m_SessionKind = 0xff;

	// 描点 = 玩家按下录制键那一刻的瞄准方向(不是保存/出手时的视角)
	GetRenderCameraAngles( m_SessionThrowAngles );
	m_SessionStartTime = Now();
	DEV_LOG( "[helper] record session started" );
}

void CHelper::UpdateRecordSession()
{
	auto* player = GetCL_Players()->GetLocalPlayerPawn();
	if ( !player || !player->IsAlive() || player->m_bIsBuyMenuOpen() )
		return;

	const std::uint32_t tick = TickCount();
	const Vector3 pos = player->GetOrigin();

	// 就绪门槛:会话开始后先等玩家"手持投掷物且基本静止"才正式记录,
	// 避免把走过去/切雷的晃动过程录进站位与动作。
	if ( !m_SessionReady )
	{
		const std::uint8_t readyKind = ResolveWeaponKind();
		const Vector3 vel = player->m_vecAbsVelocity();
		const float horizSpeedSqr = vel.m_x * vel.m_x + vel.m_y * vel.m_y;
		const bool still = std::isfinite( vel.m_x ) && horizSpeedSqr <= ( 15.f * 15.f )
			&& std::abs( vel.m_z ) <= 12.f;
		if ( readyKind != 0xff && still )
		{
			m_SessionReady = true;
			DEV_LOG( "[helper] record session ready (grenade in hand, standing still)" );
		}
		return;
	}

	// 蹲姿
	bool ducked = false;
	if ( auto* mv = player->m_pMovementServices() )
	{
		auto* ms = reinterpret_cast<CCSPlayer_MovementServices*>( mv );
		ducked = ms->m_bDucked() || ms->m_flDuckAmount() >= 0.94f;
	}

	// 空中(跳投)
	const std::uint32_t flags = player->m_fFlags();
	const Vector3 velocity = player->m_vecAbsVelocity();
	const bool airborne = ( flags & 1u ) == 0u
		|| ( std::isfinite( velocity.m_z ) && std::abs( velocity.m_z ) > 12.f );

	const std::uint8_t kind = ResolveWeaponKind();
	auto* weapon = GetCL_Weapons()->GetLocalActiveWeapon();

	if ( kind == 0xff || !weapon )
	{
		// 手里不是雷:若此前已 armed,说明刚扔出去/切走 → 出手完成
		if ( m_SessionPinArmed )
		{
			m_SessionPinArmed = false;
			m_SessionThrew = true;
			m_SessionThrowTick = tick;
			if ( m_SessionJumpTick != 0 && tick > m_SessionJumpTick )
				m_SessionAfterJumpTicks = static_cast<std::uint8_t>( std::min<std::uint32_t>( tick - m_SessionJumpTick , 255 ) );
		}
		return;
	}

	auto* grenade = reinterpret_cast<C_BaseCSGrenade*>( weapon );
	const bool pinPulled = grenade->m_bPinPulled();

	if ( !m_SessionPinArmed )
	{
		// 拔销:投掷动作开始,记录站位与雷类型
		if ( pinPulled )
		{
			m_SessionPinArmed = true;
			m_SessionArmTick = tick;
			m_SessionArmPos = pos;
			m_SessionKind = kind;
			m_SessionRunTicks = 0;
			m_SessionJumpTick = 0;
			m_SessionWasAirborne = false;
			m_SessionWasCrouch = ducked;
			m_SessionWasWalk = false;
		}
		return;
	}

	// 监视中:力度 / 跑动(按实际水平速度,不依赖 helper 按键绑定)/ 起跳
	const float strength = grenade->m_flThrowStrength();
	if ( std::isfinite( strength ) )
		m_SessionStrength = strength;

	// 拔销后在地面移动(水平速度 > 5 u/s 视为移动,低阈值避免起步漏计)
	// → 按游戏 tick 去重累计,窗口与执行端 DriveThrow 对齐(拔销 → 起跳/出手)
	if ( tick != m_SessionLastTick )
	{
		const float horizSpeedSqr = velocity.m_x * velocity.m_x + velocity.m_y * velocity.m_y;
		const bool moving = std::isfinite( velocity.m_x ) && std::isfinite( velocity.m_y )
			&& horizSpeedSqr > ( 5.f * 5.f );
		if ( moving && !airborne )
		{
			++m_SessionRunTicks;
			// 静步:跑动期间按下玩家的静步绑定键(改键后同样生效)→ action_walk
			if ( helper::g_move_walk.active() )
				m_SessionWasWalk = true;
		}
		m_SessionLastTick = tick;
	}

	// 起跳:记录首个空中 tick,并持续累积空中状态(覆盖"松手瞬间武器切走"的分支)
	if ( airborne && m_SessionJumpTick == 0 )
		m_SessionJumpTick = tick;
	if ( airborne )
		m_SessionWasAirborne = true;

	if ( m_SessionWasCrouch == false && ducked )
		m_SessionWasCrouch = true;

	// 松手(销收回)= 出手完成
	if ( !pinPulled )
	{
		m_SessionPinArmed = false;
		m_SessionThrew = true;
		m_SessionThrowTick = tick;
		if ( m_SessionJumpTick != 0 && tick > m_SessionJumpTick )
			m_SessionAfterJumpTicks = static_cast<std::uint8_t>( std::min<std::uint32_t>( tick - m_SessionJumpTick , 255 ) );
	}
}

void CHelper::EndRecordSession()
{
	SaveSessionLineup();
	m_RecordSessionActive = false;
	DEV_LOG( "[helper] record session ended" );
}

void CHelper::SaveSessionLineup()
{
	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() )
		return;

	auto* player = GetCL_Players()->GetLocalPlayerPawn();
	if ( !player || !player->IsAlive() )
		return;

	auto* recorder = GetHelperRecorder();
	if ( !recorder )
		return;

	UserLineup lineup;
	lineup.manual = true;

	if ( m_SessionThrew && m_SessionKind != 0xff )
	{
		// 会话内真实投掷:用拔销站位 + 出手视角 + 动作时序
		lineup.x = m_SessionArmPos.m_x;
		lineup.y = m_SessionArmPos.m_y;
		lineup.z = m_SessionArmPos.m_z;
		lineup.pitch = m_SessionThrowAngles.m_x;
		lineup.yaw = m_SessionThrowAngles.m_y;
		lineup.kind = m_SessionKind;

		std::uint16_t actions = 0;
		if ( m_SessionWasCrouch )
			actions |= nd::action_crouch;
		if ( m_SessionRunTicks > 0 )
			actions |= nd::action_run;
		if ( m_SessionRunTicks > 0 && m_SessionWasWalk )
			actions |= nd::action_walk;
		if ( m_SessionWasAirborne )
			actions |= nd::action_jump;
		lineup.actions = actions;

		lineup.run_ticks = std::min<std::uint32_t>( m_SessionRunTicks , 0xFFFFu );
		lineup.after_jump_ticks = m_SessionAfterJumpTicks;

		// 力度圆整到 {0.0, 0.5, 1.0}(对齐左键/右键/双键三档)
		float strength = m_SessionStrength;
		if ( strength < 0.25f )
			strength = 0.f;
		else if ( strength < 0.75f )
			strength = 0.5f;
		else
			strength = 1.f;
		lineup.throw_strength = strength;
	}
	else
	{
		// 会话内没投掷:退化为当前快照(点一下关闭也要保存)
		const std::uint8_t kind = ResolveWeaponKind();
		if ( kind == 0xff )
			return;
		const Vector3 pos = player->GetOrigin();
		lineup.x = pos.m_x;
		lineup.y = pos.m_y;
		lineup.z = pos.m_z;
		lineup.pitch = m_SessionThrowAngles.m_x;
		lineup.yaw = m_SessionThrowAngles.m_y;
		lineup.kind = kind;
		lineup.actions = 0;
		lineup.run_ticks = 0;
		lineup.after_jump_ticks = 0;
		lineup.throw_strength = 1.f;
	}

	// 动作标签 + 自动命名(对齐内置点位:run+walk 显示 Walk+,纯 run 显示 Run+)
	lineup.action = CHelperRecorder::BuildActionLabel( lineup.actions );

	// 名字:优先用面板输入框的名字,空则自动 Custom N
	if ( !m_RecordName.empty() )
		lineup.name = m_RecordName;
	else
	{
		const auto* existing = recorder->Get( mapName );
		const int count = existing ? static_cast<int>( existing->size() ) : 0;
		lineup.name = "Custom " + std::to_string( count + 1 );
	}

	const int index = recorder->Add( mapName , lineup );
	DEV_LOG( "[helper] saved session %s on %s (idx=%d kind=%u str=%.2f run=%u aj=%u)" ,
		lineup.name.c_str() , mapName.c_str() , index , lineup.kind ,
		lineup.throw_strength , lineup.run_ticks , lineup.after_jump_ticks );
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

	const std::string mapName = NormalizeMapName( mapRaw );
	const auto* map = FindMap( mapName );

	const int drawDistance = menu_state::drawDistance;
	const float maxDistanceSqr = static_cast<float>( drawDistance ) * drawDistance;

	// 用户录制点位表(含内置覆盖条目,先取出来供内置遍历时查覆盖)
	const std::vector<UserLineup>* userLineups = nullptr;
	if ( auto* recorder = GetHelperRecorder() )
		userLineups = recorder->Get( mapName );

	// 查某条内置点位是否被用户覆盖(返回覆盖条目指针,无则 nullptr)
	auto findOverride = [&]( std::uint32_t builtinIndex ) -> const UserLineup*
	{
		if ( !userLineups )
			return nullptr;
		for ( const auto& lu : *userLineups )
			if ( lu.override_builtin_index == static_cast<int>( builtinIndex ) )
				return &lu;
		return nullptr;
	};

	// 内置点位表(某些地图可能没有,此时仅用用户录制点位)。
	if ( map )
	{
		for ( std::uint32_t i = 0; i < map->count; ++i )
		{
			const auto& entry = map->data[ i ];
			if ( static_cast<std::uint8_t>( entry.type ) != weaponKind )
				continue;

			// 有覆盖:隐藏则不显示不参与;否则用覆盖版本替代内置原条目
			if ( const auto* ov = findOverride( i ) )
			{
				if ( ov->hidden )
					continue; // 该内置点位被用户隐藏
				if ( ov->kind != weaponKind )
					continue;
				const Vector3 position{ ov->x , ov->y , ov->z };
				const float distanceSqr = ( position - playerPos ).LengthSquared();
				if ( distanceSqr > maxDistanceSqr )
					continue;
				out.push_back( LineupView{
					ov->name , ov->action , position , ov->pitch , ov->yaw ,
					ov->kind ,
					ov->actions , ov->run_ticks , ov->after_jump_ticks , ov->throw_strength , ov->manual ,
					std::sqrtf( distanceSqr ) ,
				} );
				continue;
			}

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
	}

	// 用户录制点位(内置覆盖条目已在上面替代,这里只收普通录制点位)。
	if ( userLineups )
	{
		for ( const auto& lu : *userLineups )
		{
			if ( lu.override_builtin_index >= 0 )
				continue; // 覆盖条目不重复收集
			if ( lu.hidden )
				continue; // 被用户隐藏的录制点位
			if ( lu.kind != weaponKind )
				continue;

			const Vector3 position{ lu.x , lu.y , lu.z };
			const float distanceSqr = ( position - playerPos ).LengthSquared();
			if ( distanceSqr > maxDistanceSqr )
				continue;

			out.push_back( LineupView{
				lu.name , lu.action , position , lu.pitch , lu.yaw ,
				lu.kind ,
				lu.actions , lu.run_ticks , lu.after_jump_ticks , lu.throw_strength , lu.manual ,
				std::sqrtf( distanceSqr ) ,
			} );
		}
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

// ============================================================================
// 自动走位(外部注入 WASD,不写 CreateMove)
// ============================================================================
void CHelper::DriveToPoint( const LineupView& lineup , const Vector3& playerPos , const QAngle& viewAngles )
{
	// 目标方向(水平)
	Vector3 toPoint = lineup.position - playerPos;
	toPoint.m_z = 0.f;
	const float dist = toPoint.Length();
	if ( dist <= menu_state::releaseRadius )
	{
		ReleaseMovement( false );
		return;
	}

	// 视角 forward(水平)
	Vector3 forward;
	Math::AngleVectors( QAngle{ 0.f , viewAngles.m_y , 0.f } , forward );
	forward.m_z = 0.f;
	const float fl = forward.Length();
	if ( fl < 0.001f )
		return;
	forward.m_x /= fl;
	forward.m_y /= fl;

	// 目标相对视角的偏角(度);deg>0=左,<0=右(方向反时调 l/r)
	const float dot = forward.m_x * toPoint.m_x + forward.m_y * toPoint.m_y;
	const float cross = forward.m_x * toPoint.m_y - forward.m_y * toPoint.m_x;
	const float deg = std::atan2f( cross , dot ) * 180.f / std::numbers::pi_v<float>;

	const bool f = deg >= -67.5f && deg <= 67.5f;
	const bool b = !f;
	const bool l = deg >= 22.5f && deg <= 157.5f;
	const bool r = deg >= -157.5f && deg <= -22.5f;

	m_Forward.binding = ResolveBinding( InputAction::Forward );
	m_Back.binding = ResolveBinding( InputAction::Back );
	m_Left.binding = ResolveBinding( InputAction::Left );
	m_Right.binding = ResolveBinding( InputAction::Right );

	SetControl( m_Forward , f );
	SetControl( m_Back , b );
	SetControl( m_Left , l );
	SetControl( m_Right , r );
}

// 到位反向刹车:W↔S, A↔D
void CHelper::SetBrakeKeys( bool on )
{
	m_Forward.binding = ResolveBinding( InputAction::Forward );
	m_Back.binding = ResolveBinding( InputAction::Back );
	m_Left.binding = ResolveBinding( InputAction::Left );
	m_Right.binding = ResolveBinding( InputAction::Right );

	if ( on )
	{
		SetControl( m_Back , m_BrakeF );
		SetControl( m_Forward , m_BrakeB );
		SetControl( m_Right , m_BrakeL );
		SetControl( m_Left , m_BrakeR );
	}
	else
	{
		ReleaseMovement( false );
	}
}

void CHelper::ReleaseMovement( bool includeJump )
{
	if ( includeJump ) (void)SetControl( m_Jump , false );
	(void)SetControl( m_Forward , false );
	(void)SetControl( m_Back , false );
	(void)SetControl( m_Left , false );
	(void)SetControl( m_Right , false );
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
	m_LockName.clear();
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
	m_Braking = false;
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
	// 蹲投点位:先松开投掷键(投出),保留蹲姿;由 Complete 阶段延迟松蹲,避免"起来时投掷"
	if ( m_ActiveLineup.actions & nd::action_crouch )
	{
		ReleaseAttacks();
		m_ActivationLatched = true;
		m_PhaseTick = tick;
		m_PhaseStarted = Now();
		m_ThrowPhase = ThrowPhase::Complete;
		return;
	}

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
				// 内置点位模型:跑动固定向前(W),录制 run_ticks 口径与此一致
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
		// 蹲投点位:投掷后保持蹲姿一小段再起立,避免"起来时投掷"
		if ( m_ActiveLineup.actions & nd::action_crouch )
		{
			if ( now - m_PhaseStarted >= std::chrono::milliseconds( 250 ) )
				CancelThrow( true );
			return;
		}
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
	// 录制键(toggle 类型):按下开始录制会话,再按一下结束并保存。
	// 会话中跟踪移动(run_ticks)/起跳(after_jump_ticks)/蹲姿/力度/出手视角。
	const bool recActive = helper::g_record_key.active();
	if ( recActive && !m_RecordKeyPrev )
		BeginRecordSession();
	else if ( !recActive && m_RecordKeyPrev && m_RecordSessionActive )
		EndRecordSession();
	m_RecordKeyPrev = recActive;

	if ( m_RecordSessionActive )
		UpdateRecordSession();

	if ( !menu_state::helperEnabled )
	{
		// 无条件释放所有注入键(含走位方向键),避免方向键卡住
		CancelThrow( false );
		m_ActivationLatched = false;
		ResetLock();
		return;
	}

	const bool activationHeld = helper::g_helper_key.active();
	if ( !activationHeld )
	{
		// 无条件释放所有注入键(含走位方向键),避免松开热键后一直走
		CancelThrow( false );
		m_ActivationLatched = false;
		m_AimErrorX = m_AimErrorY = 0.f;
		ResetLock();
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

	const bool positionReady = ExecutionPositionReady( lineup , playerPos );

	// 自动走位:不到位时按 WASD 走向点位,到位后反向刹车(W↔S, A↔D)
	if ( menu_state::autoMove )
	{
		if ( !positionReady )
		{
			DriveToPoint( lineup , playerPos , viewAngles );
			m_Braking = false;
		}
		else if ( !m_Braking
			&& ( m_Forward.pressed || m_Back.pressed || m_Left.pressed || m_Right.pressed ) )
		{
			m_Braking = true;
			m_BrakeStart = Now();
			m_BrakeF = m_Forward.pressed;
			m_BrakeB = m_Back.pressed;
			m_BrakeL = m_Left.pressed;
			m_BrakeR = m_Right.pressed;
			// 记录刹车速度,用于动态刹车时长(速度越快刹越久)
			const Vector3 bv = player->m_vecAbsVelocity();
			m_BrakeSpeed = std::sqrtf( bv.m_x * bv.m_x + bv.m_y * bv.m_y );
			SetControl( m_Forward , false );
			SetControl( m_Back , false );
			SetControl( m_Left , false );
			SetControl( m_Right , false );
			SetBrakeKeys( true );
		}
		else if ( m_Braking )
		{
			// 速度降到静止或动态时长到就停,避免反向刹车推过头反复走位
			const Vector3 cv = player->m_vecAbsVelocity();
			const float curSpeed = std::sqrtf( cv.m_x * cv.m_x + cv.m_y * cv.m_y );
			const float brakeMs = std::clamp( m_BrakeSpeed * 0.5f , 30.f , 120.f );
			if ( curSpeed <= 12.f
				|| Now() - m_BrakeStart >= std::chrono::milliseconds( static_cast<int>( brakeMs ) ) )
			{
				ReleaseMovement( false );
				m_Braking = false;
			}
			else
			{
				SetBrakeKeys( true );
			}
		}
	}
	else
	{
		ReleaseMovement( false );
		m_Braking = false;
	}

	const Vector3 velocity = player->m_vecAbsVelocity();
	const bool stationary = std::isfinite( velocity.m_x ) && std::isfinite( velocity.m_y ) && std::isfinite( velocity.m_z )
		&& ( velocity.m_x * velocity.m_x + velocity.m_y * velocity.m_y ) <= 144.f
		&& std::abs( velocity.m_z ) <= 12.f;
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

	// 内置点位与用户录制点位都参与自动执行(manual 仅用于来源标记,不排除执行)
	if ( menu_state::autoExecute && settled )
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

	ImFont* font = g_font->f_childs.get_font();
	const float fontSize = font ? font->FontSize : 15.f;

	const std::string title = lineup.name.empty() ? "?" : lineup.name;
	const ImVec2 titleSize = font
		? font->CalcTextSizeA( fontSize , FLT_MAX , -1.f , title.c_str() )
		: ImVec2( 100.f , 20.f );

	// 副标题:投掷动作 + 距离(米)
	std::string subtitle;
	if ( menu_state::showAction && !lineup.action.empty() )
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
	// 近(stand_radius 内)主题色,远(stand_distance 内)白色
	const ImU32 color = standing ? AccentColor( 255 ) : IM_COL32( 245 , 245 , 250 , 235 );

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

		// 描点圈半径随 aim_threshold(度)线性放大:阈值越大允许误差越大,圈越大
		const float ringRadius = 4.f + menu_state::aimThreshold * 6.f;

		// 背景圈 + 瞄点圈(就绪主题色,未就绪灰色)
		drawList->AddCircleFilled( a.screen , ringRadius + 2.f , IM_COL32( 0 , 0 , 0 , 64 ) , 16 );
		const ImU32 ring = converged ? themeCol : grayCol;
		drawList->AddCircle( a.screen , ringRadius , ring , 16 , converged ? 2.f : 1.f );

		// 描点卡片(黑底圆角 + 名称/action,就绪主题色/未就绪灰色)
		ImFont* font = g_font->f_childs.get_font();
		const float fontSize = font ? font->FontSize : 14.f;
		if ( font )
		{
			const std::string name = a.lineup->name.empty() ? "?" : a.lineup->name;
			std::string action;
			if ( menu_state::showAction && !a.lineup->action.empty() )
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

// 录制状态卡片:屏幕顶部中央的红色 REC + 已录制时长(仅录制会话中显示)
void CHelper::DrawRecordStatus( ImDrawList* drawList , int screenW , int screenH ) const
{
	if ( !m_RecordSessionActive )
		return;

	ImFont* font = ImGui::GetFont();
	if ( !font )
		return;

	// 已录制时长
	const auto now = Now();
	const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>( now - m_SessionStartTime ).count();
	char timeBuf[ 32 ];
	std::snprintf( timeBuf , sizeof( timeBuf ) , "%d.%02ds" ,
		static_cast<int>( elapsed / 1000 ) , static_cast<int>( ( elapsed % 1000 ) / 10 ) );

	// 呼吸红点(未就绪常亮偏暗,就绪后闪烁)
	const float pulse = 0.5f + 0.5f * std::sinf( static_cast<float>( ImGui::GetTime() ) * 6.f );
	const float dotAlpha = m_SessionReady ? 120.f + 135.f * pulse : 150.f;
	const ImU32 dotCol = IM_COL32( 255 , 40 , 40 , static_cast<int>( dotAlpha ) );

	// 标题行:● REC  3.25s
	const std::string label = "REC";
	const float titleSize = 18.f;
	const char* timeStr = timeBuf;

	// 参数行(仅就绪后)
	std::string params;
	if ( m_SessionReady )
	{
		char buf[ 160 ];
		std::snprintf( buf , sizeof( buf ) , "run=%u  aj=%u  %s%s%s  str=%.2f" ,
			m_SessionRunTicks , m_SessionAfterJumpTicks ,
			m_SessionWasCrouch ? "crouch " : "" ,
			m_SessionWasWalk ? "walk " : "" ,
			m_SessionWasAirborne ? "jump" : "" ,
			m_SessionStrength );
		params = buf;
	}

	// 布局参数
	const float padX = 14.f;
	const float padY = 9.f;
	const float dotR = 5.f;
	const float gapTitle = 8.f;

	const ImVec2 titleSz = font->CalcTextSizeA( titleSize , FLT_MAX , -1.f , label.c_str() );
	const ImVec2 timeSz = font->CalcTextSizeA( titleSize , FLT_MAX , -1.f , timeStr );
	const float paramsSize = 14.f;
	const ImVec2 paramsSz = params.empty()
		? ImVec2{}
		: font->CalcTextSizeA( paramsSize , FLT_MAX , -1.f , params.c_str() );

	// 卡片尺寸:标题行(圆点+REC+时长)为最宽基准,参数行取较宽者
	const float titleW = dotR * 2.f + gapTitle + titleSz.x + gapTitle + timeSz.x;
	const float cardW = std::max( titleW , paramsSz.x ) + padX * 2.f;
	const float cardH = padY * 2.f + titleSz.y + ( params.empty() ? 0.f : paramsSize + 6.f );

	// 位置:底部偏上,水平居中
	const float bottomGap = 140.f;
	const ImVec2 cardPos( ( screenW - cardW ) * 0.5f , screenH - bottomGap - cardH );
	const ImVec2 cardEnd( cardPos.x + cardW , cardPos.y + cardH );

	// 半透明黑底圆角卡(无边框)
	drawList->AddRectFilled( cardPos , cardEnd , IM_COL32( 0 , 0 , 0 , 165 ) , 6.f );

	// 第一行:圆点 + REC + 时长(时长右对齐到卡片右缘)
	const float contentTop = cardPos.y + padY;
	const ImVec2 dotC( cardPos.x + padX + dotR , contentTop + titleSz.y * 0.5f );
	drawList->AddCircleFilled( dotC , dotR , dotCol );
	drawList->AddCircle( dotC , dotR , IM_COL32( 255 , 40 , 40 , 160 ) );

	const float textLeft = dotC.x + dotR + gapTitle;
	drawList->AddText( font , titleSize , { textLeft , contentTop } , IM_COL32( 255 , 90 , 90 , 255 ) , label.c_str() );

	const float timeLeft = cardEnd.x - padX - timeSz.x;
	drawList->AddText( font , titleSize , { timeLeft , contentTop } , IM_COL32( 245 , 247 , 252 , 255 ) , timeStr );

	// 第二行:实时参数(仅就绪后),浅灰白
	if ( !params.empty() )
	{
		const float paramsTop = contentTop + titleSz.y + 6.f;
		drawList->AddText( font , paramsSize , { cardPos.x + padX , paramsTop } ,
			IM_COL32( 210 , 215 , 225 , 235 ) , params.c_str() );
	}
}

// 渲染主入口
auto CHelper::OnRender( ImDrawList* drawList , int screenW , int screenH ) -> void
{
	if ( !drawList )
		return;

	Tick();

	// 录制状态卡片(与 helper 开关无关,录制会话中始终显示)
	DrawRecordStatus( drawList , screenW , screenH );

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

		ImFont* font = g_font->f_childs.get_font();
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
			std::string name = p->name.empty() ? "?" : p->name;
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
			std::string name = g.points[ i ]->name.empty() ? "?" : g.points[ i ]->name;
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

	// 站圈(参考 vesta:stand_radius 内绿,stand_distance 内蓝,更远不画)
	for ( const auto& lineup : m_RenderScratch )
	{
		if ( lineup.distance > menu_state::standDistance )
			continue;
		DrawStandMarker( drawList , lineup , lineup.distance <= menu_state::standRadius );
	}

	// 描点(当前走到的点位对应的所有描点)
	DrawMouseAimPoints( drawList , screenW , screenH );
}

auto GetHelper() -> CHelper*
{
	return &g_CHelper;
}
