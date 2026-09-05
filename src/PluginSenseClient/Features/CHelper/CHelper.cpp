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
#include <CS2/SDK/Econ/CEconItemDefinition.hpp>

#include <PluginSenseClient/Settings/MenuState.hpp>
#include <PluginSenseClient/GUI/framework_w/includes.hh>
#include <PluginSenseClient/GUI/framework_w/render/fonts/weapon_icon_map.hpp>

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

	std::string WeaponDefinitionName( int defIndex )
	{
		const auto it = kItemDefinitionNames.find( defIndex );
		if ( it != kItemDefinitionNames.end() )
			return it->second;
		return "";
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

	// 角度差 → 鼠标 counts(灵敏度 500ms 缓存 + FOV 补偿 + dither 累计)
	bool MouseDeltaCounts( float dPitch , float dYaw , int& dx , int& dy )
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

	// ---- 穿点(wallbang)武器工具 ----
	// 当前手持武器短名(ak47 / awp / m4a1_silencer ...),用于墙点图标;无则空
	std::string WeaponShortName( std::uintptr_t item )
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
	std::string WeaponIconChar( const std::string& shortName )
	{
		if ( shortName.empty() )
			return {};
		const auto it = weapon_icon_map::icon_table.find( shortName );
		return it != weapon_icon_map::icon_table.end() ? it->second : std::string();
	}

	// 从当前 active weapon 取 item(CEconItemDefinition*),穿点武器工具用
	std::uintptr_t ActiveWeaponItem()
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
	bool CurrentWeaponIsWallbang()
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
	bool WallbangWeaponsContain( const std::string& list , const std::string& shortName )
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

	// 安全读实体上的 schema 字段:地址/偏移/内存任一不可靠时返回 fallback,不裸解引用
	bool SafeReadSchemaBool( std::uintptr_t entity , const char* className , const char* propertyName , bool fallback )
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

}

static CHelper g_CHelper{};

// ============================================================================
// 视角来源(对齐项目:CreateMove 钩子传 CCSGOInput,用 CCSGOInput_GetViewAngles)
// ============================================================================
void CHelper::OnCreateMove( CCSGOInput* pInput , CUserCmd* pUserCmd )
{
	m_pInput = pInput;
	m_pCmd = pUserCmd;

	// 录制状态机按游戏 tick 驱动(纯读):按钮/视角/位置每 tick 采样一帧。
	// 会话建立/结束(含落盘)仍留在渲染侧 Tick,与菜单对录制表的读写保持同线程串行。
	if ( m_RecordSessionActive )
		UpdateRecordSession();
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

	// 投掷物 → 对应雷类型
	const std::string name = WeaponDefinitionName( item->m_iItemDefinitionIndex() );
	if ( name == "weapon_smokegrenade" ) return static_cast<std::uint8_t>( nd::kind::smoke );
	if ( name == "weapon_flashbang" ) return static_cast<std::uint8_t>( nd::kind::flash );
	if ( name == "weapon_molotov" || name == "weapon_incgrenade" ) return static_cast<std::uint8_t>( nd::kind::molotov );
	if ( name == "weapon_hegrenade" ) return static_cast<std::uint8_t>( nd::kind::he );
	if ( name == "weapon_decoy" ) return static_cast<std::uint8_t>( nd::kind::decoy );

	// 可穿墙枪械 → 穿点(墙bang)类型
	if ( CurrentWeaponIsWallbang() )
		return static_cast<std::uint8_t>( nd::kind::wallbang );

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
	case static_cast<std::uint8_t>( nd::kind::wallbang ): return "Wallbang";
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
				label += std::to_string( i + 1 ) + ". " + lu.name;
				label += " (" + KindLabel( lu.kind ) + ")";
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

// 内置点位 = 点位库时间线(只读:列表/传送;参数化编辑不适用于逐帧数据)
std::vector<std::string> CHelper::BuildBuiltinItems( std::uint8_t kindFilter ) const
{
	std::vector<std::string> items;
	if ( !helper_timeline::Ready() )
		return items;

	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() )
		return items;

	const auto* points = helper_timeline::GetMapPoints( mapName );
	if ( !points )
		return items;

	int index = 0;
	for ( const auto& point : *points )
	{
		if ( kindFilter != 0xff && point.kind != kindFilter )
			continue;

		std::string label = "[" + std::to_string( index ) + "]";
		if ( point.hidden )
			label += " [hidden]";
		label += " " + point.name;
		label += " (" + KindLabel( point.kind ) + ")";
		items.push_back( std::move( label ) );
		++index;
	}
	return items;
}

// 时间线点位以"可传送"形式读出;Save/Remove 对时间线条目为 no-op
bool CHelper::GetBuiltinItem( int listPos , std::uint8_t kindFilter , UserLineup& out ) const
{
	if ( !helper_timeline::Ready() )
		return false;

	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() || listPos < 0 )
		return false;

	const auto* points = helper_timeline::GetMapPoints( mapName );
	if ( !points )
		return false;

	int matched = 0;
	for ( const auto& point : *points )
	{
		if ( kindFilter != 0xff && point.kind != kindFilter )
			continue;

		if ( matched == listPos )
		{
			const QAngle aimAngles = !point.frames.empty() ? point.frames.front().angles : point.angles;
			out = UserLineup{};
			out.name = point.name;
			out.weapon = point.weapon;
			out.x = point.position.m_x;
			out.y = point.position.m_y;
			out.z = point.position.m_z;
			out.pitch = aimAngles.m_x;
			out.yaw = aimAngles.m_y;
			out.kind = point.kind;
			out.hidden = point.hidden;
			out.builtin_id = point.id; // Save(隐藏)与编辑缓冲的 original 用它定位
			return true;
		}
		++matched;
	}
	return false;
}

// 内置时间线点位:仅支持隐藏覆盖(helper_lineups.dat 里写一条 builtin_id 条目)
bool CHelper::SaveBuiltinOverride( int builtinIndex , const UserLineup& lineup )
{
	auto* recorder = GetHelperRecorder();
	if ( !recorder || builtinIndex < 0 )
		return false;

	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() )
		return false;

	auto* point = helper_timeline::FindPointById( builtinIndex );
	if ( !point )
		return false;

	// 已有同 id 的覆盖条目 → 更新;无则追加(保留其它字段以便取消隐藏时还原)
	const auto* existing = recorder->Get( mapName );
	if ( !existing )
	{
		UserLineup entry;
		entry.builtin_id = builtinIndex;
		entry.hidden = lineup.hidden;
		entry.kind = point->kind;
		entry.name = point->name;
		recorder->Add( mapName , entry );
		point->hidden = lineup.hidden;
		return true;
	}
	auto& list = const_cast<std::vector<UserLineup>&>( *existing );
	for ( auto& lu : list )
	{
		if ( lu.builtin_id == builtinIndex )
		{
			lu.hidden = lineup.hidden;
			lu.name = point->name;
			recorder->Save();
			point->hidden = lineup.hidden;
			return true;
		}
	}

	// 列表存在但没有该 id 的覆盖条目 → 追加
	UserLineup entry;
	entry.builtin_id = builtinIndex;
	entry.hidden = lineup.hidden;
	entry.kind = point->kind;
	entry.name = point->name;
	recorder->Add( mapName , entry );
	point->hidden = lineup.hidden;
	return true;
}

bool CHelper::RemoveBuiltinOverride( int builtinIndex )
{
	auto* recorder = GetHelperRecorder();
	if ( !recorder || builtinIndex < 0 )
		return false;

	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() )
		return false;

	if ( const auto* list = recorder->Get( mapName ) )
	{
		for ( std::size_t i = 0; i < list->size(); ++i )
		{
			if ( ( *list )[ i ].builtin_id == builtinIndex )
			{
				recorder->Remove( mapName , i );
				if ( auto* point = helper_timeline::FindPointById( builtinIndex ) )
					point->hidden = false;
				return true;
			}
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
// 墙点武器多选:可穿墙枪械清单(短名 + 显示名)
// ============================================================================
const std::vector<CHelper::WallWeaponOption>& CHelper::WallWeaponOptions()
{
	static const std::vector<WallWeaponOption> options =
	{
		// 手枪
		{ "glock" , "Glock" } , { "hkp2000" , "P2000" } , { "usp_silencer" , "USP-S" } ,
		{ "p250" , "P250" } , { "elite" , "Dual" } , { "fiveseven" , "Five-SeveN" } ,
		{ "tec9" , "Tec-9" } , { "cz75a" , "CZ75" } , { "revolver" , "R8" } ,
		{ "deagle" , "Deagle" } ,
		// SMG
		{ "mac10" , "Mac-10" } , { "mp9" , "MP9" } , { "mp7" , "MP7" } ,
		{ "mp5sd" , "MP5-SD" } , { "ump45" , "UMP-45" } , { "p90" , "P90" } ,
		{ "bizon" , "PP-Bizon" } ,
		// 步枪
		{ "galilar" , "Galil" } , { "famas" , "FAMAS" } , { "ak47" , "AK-47" } ,
		{ "m4a1" , "M4A4" } , { "m4a1_silencer" , "M4A1-S" } , { "sg556" , "SG 553" } ,
		{ "aug" , "AUG" } ,
		// 狙击
		{ "ssg08" , "SSG 08" } , { "awp" , "AWP" } , { "scar20" , "SCAR-20" } ,
		{ "g3sg1" , "G3SG1" } ,
		// 霰弹
		{ "nova" , "Nova" } , { "xm1014" , "XM1014" } , { "mag7" , "MAG-7" } ,
		{ "sawedoff" , "Sawed-Off" } ,
		// 机枪
		{ "m249" , "M249" } , { "negev" , "Negev" } ,
	};
	return options;
}

void CHelper::ParseWallWeapons( const std::string& list , std::vector<bool>& selected )
{
	selected.assign( WallWeaponOptions().size() , false );
	if ( list.empty() )
		return;
	const auto& options = WallWeaponOptions();
	std::size_t start = 0;
	while ( start <= list.size() )
	{
		const auto comma = list.find( ',' , start );
		const auto part = comma == std::string::npos
			? list.substr( start ) : list.substr( start , comma - start );
		for ( std::size_t i = 0; i < options.size(); ++i )
			if ( part == options[ i ].shortName )
				selected[ i ] = true;
		if ( comma == std::string::npos )
			break;
		start = comma + 1;
	}
}

std::string CHelper::BuildWallWeapons( const std::vector<bool>& selected )
{
	std::string out;
	const auto& options = WallWeaponOptions();
	for ( std::size_t i = 0; i < options.size() && i < selected.size(); ++i )
	{
		if ( !selected[ i ] )
			continue;
		if ( !out.empty() )
			out += ",";
		out += options[ i ].shortName;
	}
	return out;
}

// 与 WallWeaponOptions 对齐的 esp 图标字符(空串 = 无图标),供编辑面板武器多选框行前图标
const std::vector<std::string>& CHelper::WallWeaponIcons()
{
	static std::vector<std::string> icons = []()
	{
		std::vector<std::string> out;
		const auto& options = WallWeaponOptions();
		out.reserve( options.size() );
		for ( const auto& opt : options )
			out.push_back( WeaponIconChar( opt.shortName ) );
		return out;
	}();
	return icons;
}

// ============================================================================
// 录制会话(toggle 键:按下开始,再按一下保存)
// 时间线录制:就绪(手持武器且静止)后每 usercmd 采样一帧
// {按钮, 视角, 位置},出手(攻击松开)后再录 32 tick 尾巴即完成 —— 无参数推断。
// ============================================================================
void CHelper::BeginRecordSession()
{
	m_RecordSessionActive = true;
	m_SessionReady = false;
	m_SessionFrames.clear();
	m_SessionSawAttack = false;
	m_SessionTail = -1;
	m_SessionTailDone = false;
	m_SessionKind = 0xff;
	m_SessionArmPos = {};
	m_SessionThrowAngles = {};
	m_SessionStartTime = Now();
}

void CHelper::UpdateRecordSession()
{
	auto* player = GetCL_Players()->GetLocalPlayerPawn();
	if ( !player || !player->IsAlive() || player->m_bIsBuyMenuOpen() )
		return;

	const std::uint32_t tick = TickCount();

	// 就绪门槛:手持可用武器(雷或枪)且基本静止,避免把走过去/切雷录进起点
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
			m_SessionKind = readyKind;
			m_SessionArmPos = player->GetOrigin();
			GetRenderCameraAngles( m_SessionThrowAngles );
			m_SessionFrames.clear();
			m_SessionSawAttack = false;
			m_SessionTail = -1;
			m_SessionTailDone = false;
			m_SessionLastTick = tick;
		}
		return;
	}

	// 尾巴完成:等渲染侧落盘(与菜单同线程),不再采样
	if ( m_SessionTailDone )
		return;

	// 每 usercmd 一帧(tick 去重)
	if ( tick == m_SessionLastTick )
		return;
	m_SessionLastTick = tick;

	helper_timeline::Frame frame;
	if ( !GetRenderCameraAngles( frame.angles ) )
		frame.angles = m_SessionThrowAngles;
	frame.position = player->GetOrigin();

	// 按钮位:从本 command 的 button_states 读受管 10 位(纯读)
	if ( m_pCmd )
	{
		const std::uint64_t buttons = m_pCmd->button_states.buttonstate1;
		frame.in_attack    = ( buttons & IN_ATTACK ) != 0;
		frame.in_attack2   = ( buttons & IN_ATTACK2 ) != 0;
		frame.in_jump      = ( buttons & IN_JUMP ) != 0;
		frame.in_duck      = ( buttons & IN_DUCK ) != 0;
		frame.in_forward   = ( buttons & IN_FORWARD ) != 0;
		frame.in_back      = ( buttons & IN_BACK ) != 0;
		frame.in_use       = ( buttons & IN_USE ) != 0;
		frame.in_moveleft  = ( buttons & IN_MOVELEFT ) != 0;
		frame.in_moveright = ( buttons & IN_MOVERIGHT ) != 0;
		frame.in_speed     = ( buttons & IN_SPEED ) != 0;
	}

	m_SessionFrames.push_back( frame );

	if ( frame.in_attack )
		m_SessionSawAttack = true;

	// 雷类会话:攻击松开(雷离手)后录 32 tick 尾巴即完成
	const bool isNade = m_SessionKind != static_cast<std::uint8_t>( resources::nades::kind::wallbang );
	if ( isNade && m_SessionSawAttack && !frame.in_attack && m_SessionTail < 0 )
	{
		m_SessionTail = 32;
	}

	if ( isNade && m_SessionTail > 0 )
	{
		--m_SessionTail;
		if ( m_SessionTail == 0 )
			m_SessionTailDone = true; // 渲染侧 Tick 负责落盘
	}
}

void CHelper::EndRecordSession()
{
	SaveSessionLineup();
	m_RecordSessionActive = false;
}

void CHelper::SaveSessionLineup()
{
	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() )
		return;

	auto* recorder = GetHelperRecorder();
	if ( !recorder )
		return;

	UserLineup lineup;
	lineup.kind = m_SessionKind;
	lineup.x = m_SessionArmPos.m_x;
	lineup.y = m_SessionArmPos.m_y;
	lineup.z = m_SessionArmPos.m_z;
	lineup.pitch = m_SessionThrowAngles.m_x;
	lineup.yaw = m_SessionThrowAngles.m_y;

	if ( m_SessionKind == static_cast<std::uint8_t>( resources::nades::kind::wallbang ) )
	{
		// 墙点:纯站位快照(无帧);默认勾选当前手持枪
		lineup.weapon = WeaponShortName( ActiveWeaponItem() );
		if ( lineup.name.empty() )
		{
			const auto* existing = recorder->Get( mapName );
			int wallCount = 1;
			if ( existing )
				for ( const auto& lu : *existing )
					if ( lu.kind == static_cast<std::uint8_t>( resources::nades::kind::wallbang ) )
						++wallCount;
			lineup.name = "Wall " + std::to_string( wallCount );
		}
	}
	else
	{
		// 雷类:时间线点位(要求真的出了手;没出手不保存)
		if ( m_SessionFrames.empty() || !m_SessionSawAttack || m_SessionTail < 0 )
		{
			DEV_LOG( "[helper] nade session discarded (no throw)" );
			return;
		}
		lineup.frames = m_SessionFrames;
		if ( lineup.name.empty() )
		{
			const auto* existing = recorder->Get( mapName );
			const int count = existing ? static_cast<int>( existing->size() ) : 0;
			lineup.name = "Custom " + std::to_string( count + 1 );
		}
	}

	const int index = recorder->Add( mapName , lineup );
	DEV_LOG( "[helper] saved %s on %s (idx=%d kind=%u frames=%zu)" ,
		lineup.name.c_str() , mapName.c_str() , index , lineup.kind , lineup.frames.size() );
}

// ============================================================================
// 墙点执行:走到位 → 瞄准 → 就绪。Crouch/Jump 动作只作点位标注(名牌/描点文字),
// 执行端不注入任何蹲/跳/助跑键,开枪交给 rage/玩家。
// ============================================================================
void CHelper::ResetWallbangAction()
{
	m_WallPhase = WallPhase::Idle;
}

void CHelper::DriveWallbang( const Vector3& playerPos , const QAngle& viewAngles , std::uint32_t tick , std::chrono::steady_clock::time_point now )
{
	// 收集当前手持武器对应的墙点(用户录制,kind=wallbang,且武器集合含当前枪)
	if ( !Collect( playerPos , static_cast<std::uint8_t>( nd::kind::wallbang ) , m_TickScratch ) )
	{
		ResetLock();
		ResetWallbangAction();
		return;
	}

	const int index = SelectArmed( m_TickScratch , viewAngles );
	if ( index < 0 )
	{
		m_AimErrorX = m_AimErrorY = 0.f;
		ResetLock();
		ResetWallbangAction();
		return;
	}

	const auto& lineup = m_TickScratch[ static_cast<std::size_t>( index ) ];
	float error = AngleError( viewAngles , lineup.pitch , lineup.yaw );
	if ( menu_state::autoAim )
		AimAt( lineup , viewAngles , error );

	const bool positionReady = ExecutionPositionReady( lineup , playerPos );

	// 自动走位:不到位时走向点位,到位反向刹车(与投掷点位同一套)
	if ( menu_state::autoMove )
	{
		if ( !positionReady )
		{
			DriveToPoint( lineup , playerPos , viewAngles );
			m_Braking = false;
			ResetWallbangAction(); // 未到位前不做动作播放
		}
		else if ( !m_Braking
			&& ( m_Forward.pressed || m_Back.pressed || m_Left.pressed || m_Right.pressed ) )
		{
			m_Braking = true;
			m_BrakeStart = now;
			m_BrakeF = m_Forward.pressed;
			m_BrakeB = m_Back.pressed;
			m_BrakeL = m_Left.pressed;
			m_BrakeR = m_Right.pressed;
			const Vector3 bv = GetCL_Players()->GetLocalPlayerPawn()->m_vecAbsVelocity();
			m_BrakeSpeed = std::sqrtf( bv.m_x * bv.m_x + bv.m_y * bv.m_y );
			SetControl( m_Forward , false );
			SetControl( m_Back , false );
			SetControl( m_Left , false );
			SetControl( m_Right , false );
			SetBrakeKeys( true );
		}
		else if ( m_Braking )
		{
			const Vector3 cv = GetCL_Players()->GetLocalPlayerPawn()->m_vecAbsVelocity();
			const float curSpeed = std::sqrtf( cv.m_x * cv.m_x + cv.m_y * cv.m_y );
			const float brakeMs = std::clamp( m_BrakeSpeed * 0.5f , 30.f , 120.f );
			if ( curSpeed <= 12.f
				|| now - m_BrakeStart >= std::chrono::milliseconds( static_cast<int>( brakeMs ) ) )
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
		m_Coasting = false;
	}

	// 站定后即就绪:动作(Crouch/Jump)只是标注,执行端不注入任何键,等 rage/玩家开火
	if ( positionReady && !m_Braking )
		m_WallPhase = WallPhase::Ready;

	// 就绪 = 走位到位 + 视角对准;完成后不再注入,交给 rage/玩家开火
	if ( m_WallPhase == WallPhase::Ready )
	{
		if ( error <= menu_state::aimThreshold )
		{
			m_AimErrorX = m_AimErrorY = 0.f;
			m_LastAimUpdate = {};
		}
	}
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

	const int drawDistance = menu_state::drawDistance;
	const float maxDistanceSqr = static_cast<float>( drawDistance ) * drawDistance;

	// 用户录制点位表(含内置覆盖条目,先取出来供内置遍历时查覆盖)
	const std::vector<UserLineup>* userLineups = nullptr;
	if ( auto* recorder = GetHelperRecorder() )
		userLineups = recorder->Get( mapName );

	// 点位库(时间线):替代内置参数表,按地图 + 武器类型 + 距离过滤
	if ( helper_timeline::Ready() )
	{
		if ( const auto* points = helper_timeline::GetMapPoints( mapName ) )
		{
			for ( const auto& point : *points )
			{
				if ( point.kind != weaponKind )
					continue;
				if ( point.hidden )
					continue; // 被用户隐藏的内置点位

				const float distanceSqr = ( point.position - playerPos ).LengthSquared();
				if ( distanceSqr > maxDistanceSqr )
					continue;

				// 瞄准目标 = 首帧视角(回放会逐帧驱动视角到各帧角度)
				const QAngle aimAngles = !point.frames.empty() ? point.frames.front().angles : point.angles;

				LineupView view;
				view.name = point.name;
				view.position = point.position;
				view.pitch = aimAngles.m_x;
				view.yaw = aimAngles.m_y;
				view.kind = point.kind;
				view.distance = std::sqrtf( distanceSqr );
				view.frames = point.frames.data();
				view.frameCount = point.frames.size();
				out.push_back( std::move( view ) );
			}
		}
	}

	// 用户录制点位(内置覆盖条目已在上面替代,这里只收普通录制点位)。
	if ( userLineups )
	{
		for ( const auto& lu : *userLineups )
		{
			if ( lu.hidden )
				continue; // 被用户隐藏的录制点位
			if ( lu.kind != weaponKind )
				continue;

			// 墙点:weapon = 逗号分隔的可用枪列表;不含当前手持枪则不显示/参与
			if ( lu.kind == static_cast<std::uint8_t>( nd::kind::wallbang ) )
			{
				const std::string cur = WeaponShortName( ActiveWeaponItem() );
				if ( !WallbangWeaponsContain( lu.weapon , cur ) )
					continue;
			}

			const Vector3 position{ lu.x , lu.y , lu.z };
			const float distanceSqr = ( position - playerPos ).LengthSquared();
			if ( distanceSqr > maxDistanceSqr )
				continue;

			LineupView view;
			view.name = lu.name;
			view.position = position;
			view.pitch = lu.pitch;
			view.yaw = lu.yaw;
			view.kind = lu.kind;
			view.distance = std::sqrtf( distanceSqr );
			view.weapon = lu.weapon;      // 墙点:存武器集合(名牌取当前手持枪图标)
			view.annotations = lu.annotations; // 墙点:Crouch/Jump 标注
			if ( !lu.frames.empty() )
			{
				// 时间线点位(自录):走时间线回放引擎
				view.frames = lu.frames.data();
				view.frameCount = lu.frames.size();
			}
			out.push_back( std::move( view ) );
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

// 卸载路径:释放当前注入按住的所有键并复位状态机(CDllLauncher::OnDestroy 调用)
auto CHelper::OnUnload() -> void
{
	CancelThrow( false );
	ResetWallbangAction();
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
	m_AimErrorX = m_AimErrorY = 0.f;
	m_LastAimUpdate = {};
	m_Braking = false;
	ResetLock();
	// 时间线回放一并复位(所有守卫路径都经这里,保证按钮掩码被释放)
	m_TimelineActive = false;
	m_TimelineFrames.clear();
	m_TimelineName.clear();
	m_TimelineKind = 0xff;
	m_TimelineInjected = 0;
	m_TimelineFirstAttack = 0;
	m_TimelineStartTickSet = false;
	m_ActivationLatched = latch;
}

// ============================================================================
// 时间线回放(点位库):逐帧按钮 diff 注入 + 视角闭环注入,全部外部输出
// ============================================================================
OwnedControl* CHelper::TimelineControl( std::uint64_t bit )
{
	switch ( bit )
	{
	case IN_ATTACK:    return &m_Attack;
	case IN_ATTACK2:   return &m_Attack2;
	case IN_JUMP:      return &m_Jump;
	case IN_DUCK:      return &m_Duck;
	case IN_FORWARD:   return &m_Forward;
	case IN_BACK:      return &m_Back;
	case IN_MOVELEFT:  return &m_Left;
	case IN_MOVERIGHT: return &m_Right;
	case IN_SPEED:     return &m_Walk;
	}
	return nullptr;
}

namespace
{
	InputAction TimelineActionOf( std::uint64_t bit )
	{
		switch ( bit )
		{
		case IN_ATTACK:    return InputAction::Attack;
		case IN_ATTACK2:   return InputAction::Attack2;
		case IN_JUMP:      return InputAction::Jump;
		case IN_DUCK:      return InputAction::Duck;
		case IN_FORWARD:   return InputAction::Forward;
		case IN_BACK:      return InputAction::Back;
		case IN_MOVELEFT:  return InputAction::Left;
		case IN_MOVERIGHT: return InputAction::Right;
		case IN_SPEED:     return InputAction::Walk;
		}
		return InputAction::Forward;
	}
}

void CHelper::ApplyTimelineButtons( std::uint64_t target )
{
	static constexpr std::uint64_t kBits[] =
		{ IN_ATTACK, IN_ATTACK2, IN_JUMP, IN_DUCK, IN_FORWARD, IN_BACK, IN_MOVELEFT, IN_MOVERIGHT, IN_SPEED };

	for ( const std::uint64_t bit : kBits )
	{
		const bool want = ( target & bit ) != 0;
		const bool have = ( m_TimelineInjected & bit ) != 0;
		if ( want == have )
			continue;

		if ( OwnedControl* control = TimelineControl( bit ) )
		{
			control->binding = ResolveBinding( TimelineActionOf( bit ) );
			SetControl( *control , want );
		}
		m_TimelineInjected = want ? ( m_TimelineInjected | bit ) : ( m_TimelineInjected & ~bit );
	}
}

void CHelper::StartTimelinePlayback( const helper_timeline::Frame* frames , std::size_t count ,
	const std::string& name , std::uint8_t kind )
{
	CancelThrow( false );
	if ( !frames || count == 0 )
	{
		m_ActivationLatched = true;
		return;
	}

	m_TimelineActive = true;
	m_TimelineFrames.assign( frames , frames + count );
	m_TimelineName = name;
	m_TimelineKind = kind;
	m_TimelineStartTickSet = false;
	m_TimelineLastTick = 0;
	m_TimelineInjected = 0;
	m_TimelineFirstAttack = 0;

	DEV_LOG( "[timeline] start '%s' frames=%zu" , name.c_str() , count );
}

void CHelper::CancelTimelinePlayback()
{
	if ( m_TimelineActive )
		CancelThrow( false );
}

void CHelper::UpdateTimelinePlayback()
{
	if ( !m_TimelineActive || m_TimelineFrames.empty() )
		return;

	auto* player = GetCL_Players()->GetLocalPlayerPawn();
	if ( !player || !player->IsAlive() )
	{
		CancelThrow( true );
		return;
	}

	const std::uint32_t tick = TickCount();
	if ( !m_TimelineStartTickSet )
	{
		m_TimelineStartTick = tick;
		m_TimelineStartTickSet = true;
		m_TimelineLastTick = tick;
	}

	// 同一游戏 tick 的多次渲染:不推帧(按钮掩码按当前帧保持)
	if ( tick == m_TimelineLastTick )
		return;

	const int elapsed = static_cast<int>( tick - m_TimelineStartTick );
	m_TimelineLastTick = tick;

	const auto& frames = m_TimelineFrames;
	if ( elapsed >= static_cast<int>( frames.size() ) )
	{
		ApplyTimelineButtons( 0 ); // 全部松开
		DEV_LOG( "[timeline] finished '%s'" , m_TimelineName.c_str() );
		CancelThrow( true );
		return;
	}

	const auto& frame = frames[ static_cast<std::size_t>( elapsed ) ];

	// firstAttackStep 保护:首个攻击帧起强制保持攻击,直到帧序列明确松开
	// (防录制帧序列 attack 不完整导致提前投掷)
	if ( m_TimelineFirstAttack == 0 )
	{
		m_TimelineFirstAttack = -1;
		for ( std::size_t i = 0; i < frames.size(); ++i )
		{
			if ( frames[ i ].in_attack || frames[ i ].in_attack2 )
			{
				m_TimelineFirstAttack = static_cast<int>( i ) + 1;
				break;
			}
		}
	}

	std::uint64_t mask = 0;
	if ( frame.in_attack )    mask |= IN_ATTACK;
	if ( frame.in_attack2 )   mask |= IN_ATTACK2;
	if ( frame.in_jump )      mask |= IN_JUMP;
	if ( frame.in_duck )      mask |= IN_DUCK;
	if ( frame.in_forward )   mask |= IN_FORWARD;
	if ( frame.in_back )      mask |= IN_BACK;
	if ( frame.in_moveleft )  mask |= IN_MOVELEFT;
	if ( frame.in_moveright ) mask |= IN_MOVERIGHT;
	if ( frame.in_speed )     mask |= IN_SPEED;
	if ( frame.in_use )       mask |= IN_USE;

	if ( m_TimelineFirstAttack > 0 && m_TimelineFirstAttack <= elapsed + 1 )
	{
		mask |= IN_ATTACK | IN_ATTACK2;
		if ( !frame.in_attack )
			mask &= ~IN_ATTACK;
		if ( !frame.in_attack2 )
			mask &= ~IN_ATTACK2;
	}

	ApplyTimelineButtons( mask );

	// 视角:每渲染帧向该帧录制视角做闭环注入(绝对目标,无漂移)
	if ( QAngle current; GetRenderCameraAngles( current ) )
	{
		const float dP = frame.angles.m_x - current.m_x;
		const float dY = WrapYaw( frame.angles.m_y - current.m_y );
		int dx = 0 , dy = 0;
		if ( MouseDeltaCounts( dP , dY , dx , dy ) && ( dx != 0 || dy != 0 ) )
			InjectMouse( dx , dy , MOUSEEVENTF_MOVE );
	}

	// 漂移检测:实际位置偏离录制轨迹过多即中止(点位数据 p 覆盖率 100%)
	if ( frame.position.LengthSquared() > 0.01f )
	{
		const Vector3 delta = player->GetOrigin() - frame.position;
		if ( delta.LengthSquared() > 48.f * 48.f )
		{
			DEV_LOG( "[timeline] drift %.1fu @frame %d,abort '%s'" ,
				delta.Length() , elapsed , m_TimelineName.c_str() );
			ApplyTimelineButtons( 0 );
			CancelThrow( true );
		}
	}
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
// 手雷轨迹 PiP 预览:状态机派生输出(锁定待投 或 投掷执行中),每帧 Tick 开头调用
// ============================================================================
static constexpr int kPreviewHoldMs = 500;

void CHelper::UpdateGrenadePreview()
{
	// ① 锁定待投: 已锁定 且 未 latch(出手前),与 Tick 里 settled 同口径
	const bool lockedReady = m_LockStarted != std::chrono::steady_clock::time_point{}
		&& !m_ActivationLatched
		&& Now() - m_LockStarted >= std::chrono::milliseconds(
			std::clamp( menu_state::lockTimeMs , 0 , 250 ) );

	// ② 投掷状态机运行中(Crouching→Priming→Running→Jumping→Complete)
	const bool throwing = m_TimelineActive; // 时间线回放中(含动作阶段)

	// 条件满足 = 想开,并刷新"最后激活时刻"
	const bool wantOn = menu_state::grenadePreview && ( lockedReady || throwing );
	const auto now = Now();
	if ( wantOn )
		m_PreviewLastActive = now;

	const bool on = menu_state::grenadePreview
		&& ( wantOn
			|| ( m_PreviewLastActive != std::chrono::steady_clock::time_point{}
				&& now - m_PreviewLastActive < std::chrono::milliseconds( kPreviewHoldMs ) ) );

	WriteGrenadePreview( on );
}

void CHelper::WriteGrenadePreview( bool on )
{
	// 给 convar 补 FCVAR_CLIENTCMD_CAN_EXECUTE 标志 + 每帧直写值,防 replicated 被服务器刷回
	static CConVar* s_pipreview = nullptr;
	if ( !s_pipreview )
		s_pipreview = SDK::Interfaces::EngineCvar()->Find( "sv_grenade_trajectory_prac_pipreview" );
	if ( s_pipreview )
	{
		s_pipreview->nFlags |= FCVAR_CLIENTCMD_CAN_EXECUTE;   // 允许客户端设置
		s_pipreview->value.i1 = on;           // 开=1,关=0(每帧保活/还原)
	}
}

// ============================================================================
// 主循环
// ============================================================================
void CHelper::Tick()
{
	// 手雷轨迹 PiP 预览:每帧开头统一派生(7 个提前 return 路径之后都不需要再管)
	UpdateGrenadePreview();

	// 录制键(toggle 会话,雷与墙点一致):按下开始,再按结束保存。
	// 会话内的状态采样在 OnCreateMove 每 tick 执行;边沿检测留在渲染侧,
	// 避免与菜单(也会轮询该 toggle 键)及录制表读写产生跨线程竞争。
	const bool recActive = helper::g_record_key.active();
	if ( recActive && !m_RecordKeyPrev )
		BeginRecordSession();
	else if ( !recActive && m_RecordKeyPrev && m_RecordSessionActive )
		EndRecordSession();
	m_RecordKeyPrev = recActive;

	if ( !menu_state::helperEnabled )
	{
		// 无条件释放所有注入键(含走位方向键),避免方向键卡住
		CancelThrow( false );
		ResetWallbangAction();
		m_ActivationLatched = false;
		ResetLock();
		return;
	}

	const bool activationHeld = helper::g_helper_key.active();
	if ( !activationHeld )
	{
		// 无条件释放所有注入键(含走位方向键),避免松开热键后一直走
		CancelThrow( false );
		ResetWallbangAction();
		m_ActivationLatched = false;
		m_AimErrorX = m_AimErrorY = 0.f;
		ResetLock();
		return;
	}

	auto* player = GetCL_Players()->GetLocalPlayerPawn();
	if ( !player || !player->IsAlive() || !GameHasInputFocus() || player->m_bIsBuyMenuOpen() )
	{
		CancelThrow( activationHeld );
		ResetWallbangAction();
		m_AimErrorX = m_AimErrorY = 0.f;
		return;
	}

	// 回合冻结/回合介绍/暂停时不执行
	if ( void* rules = SDK::Pointers::GameRules() )
	{
		auto* csRules = reinterpret_cast<C_CSGameRules*>( rules );
		if ( csRules->m_bFreezePeriod() || csRules->m_bTeamIntroPeriod() || csRules->m_bGamePaused() )
		{
			CancelThrow( activationHeld );
			ResetWallbangAction();
			m_AimErrorX = m_AimErrorY = 0.f;
			return;
		}
	}

	const std::uint8_t kind = ResolveWeaponKind();

	// ---- 墙点(穿点):走位/动作复现,不投掷不开枪 ----
	if ( kind == static_cast<std::uint8_t>( nd::kind::wallbang ) )
	{
		const Vector3 wallPlayerPos = player->GetOrigin();
		QAngle wallView;
		if ( !GetRenderCameraAngles( wallView ) )
		{
			m_AimErrorX = m_AimErrorY = 0.f;
			ResetWallbangAction();
			return;
		}
		DriveWallbang( wallPlayerPos , wallView , TickCount() , Now() );
		return;
	}

	if ( kind == 0xff )
	{
		CancelThrow( activationHeld );
		return;
	}

	// 投掷路径需要 active weapon(墙点已在上方处理并返回)
	auto* weapon = GetCL_Weapons()->GetLocalActiveWeapon();
	if ( !weapon )
	{
		CancelThrow( activationHeld );
		return;
	}

	// ---- 时间线回放(点位库):激活期间独占执行路径 ----
	if ( m_TimelineActive )
	{
		if ( kind != m_TimelineKind )
		{
			// 切枪/雷已扔出(武器类型变化):中止并锁存
			CancelThrow( true );
			return;
		}
		UpdateTimelinePlayback();
		return;
	}

	const std::uint32_t tick = TickCount();

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

	// 自动走位:不到位时按 WASD 走向点位,到位后反向刹车(W↔S, A↔D)。
	// 高速减速带:预测"松手停点"会落进出手圈时,提前松键靠摩擦滑行减速 ——
	// 进圈时速度已降到慢走量级,反向刹车的滑过误差缩小到 ~1/5
	if ( menu_state::autoMove )
	{
		if ( !positionReady )
		{
			// 减速带判定:4-tick 速度外推的预测停点进入圈(或已越过目标)→ 滑行
			Vector3 toPoint = lineup.position - playerPos;
			toPoint.m_z = 0.f;
			const float dist = toPoint.Length();
			bool wantCoast = false;
			{
				const Vector3 vel = player->m_vecAbsVelocity();
				const float speed = std::sqrtf( vel.m_x * vel.m_x + vel.m_y * vel.m_y );
				const Vector3 predicted = playerPos + vel * ( 4.f / 64.f );
				Vector3 toPredicted = lineup.position - predicted;
				toPredicted.m_z = 0.f;
				const float predictedDist = toPredicted.Length();

				// 预测停点入圈,或已越过目标(冲头)且速度尚高 → 滑行减速
				if ( predictedDist <= menu_state::releaseRadius
					|| ( predictedDist > dist && speed > 100.f ) )
					wantCoast = true;
			}

			if ( wantCoast )
			{
				ReleaseMovement( false );
				// 滑行超时兜底:0.5s 没减完就强制回粗走
				if ( m_CoastStart == std::chrono::steady_clock::time_point{} )
					m_CoastStart = Now();
				if ( Now() - m_CoastStart > std::chrono::milliseconds( 500 ) )
				{
					m_Coasting = false;
					m_CoastStart = {};
				}
			}
			else
			{
				m_Coasting = false;
				m_CoastStart = {};
				DriveToPoint( lineup , playerPos , viewAngles );
			}
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

	// 点位(时间线/用户自录)都参与自动执行
	if ( menu_state::autoExecute && settled )
	{
		if ( lineup.frames && lineup.frameCount > 0 )
			StartTimelinePlayback( lineup.frames , lineup.frameCount , lineup.name , lineup.kind );
		else
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

// 名牌图标字符:墙点画"当前手持武器"的图标(Collect 已按当前枪过滤,显示即匹配);
// 投掷点不在这走(投掷图标由 HelperKindIcon 提供)
std::string CHelper::LineupIconChar( const LineupView& lineup ) const
{
	if ( lineup.kind == static_cast<std::uint8_t>( nd::kind::wallbang ) )
		return WeaponIconChar( WeaponShortName( ActiveWeaponItem() ) );
	return {};
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
			// 墙点标注(Crouch/Jump)副标题
			std::string action;
			if ( a.lineup->kind == static_cast<std::uint8_t>( nd::kind::wallbang ) )
			{
				if ( a.lineup->annotations & nd::action_crouch )
					action = "Crouch";
				if ( a.lineup->annotations & nd::action_jump )
					action = action.empty() ? "Jump" : action + "+Jump";
			}
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

	// 参数行(仅就绪后):帧数 + 出手状态
	std::string params;
	if ( m_SessionReady )
	{
		char buf[ 160 ];
		std::snprintf( buf , sizeof( buf ) , "frames=%zu  %s" ,
			m_SessionFrames.size() ,
			m_SessionTail >= 0 ? "thrown" : "armed" );
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

		// 图标(投掷物;墙点组用组内首点的武器图标)——先量宽再布局
		const char* icon = g.kind == static_cast<std::uint8_t>( nd::kind::wallbang )
			? nullptr : HelperKindIcon( g.kind );
		const std::string weaponIcon = !g.points.empty()
			? LineupIconChar( *g.points.front() ) : std::string();
		ImFont* iconFont = g_font ? g_font->f_weapon_icons.get_font() : nullptr;
		const float iconFontSize = 14.f;
		const char* iconText = icon ? icon : weaponIcon.c_str();
		const bool hasIcon = ( icon || !weaponIcon.empty() ) && iconFont;

		// 图标槽:宽度基准 18,字形更宽随之加宽;高度固定 16,不随字形撑高名牌。
		// 枪械字形(墙点)宽高比大,按目标宽 22 反算字号,不同枪的图标宽度保持一致。
		const float iconBase = 18.f;
		const float iconH = 16.f;
		float drawIconSize = iconFontSize;
		float iconW = iconBase;
		ImVec2 iconTextSize{};
		if ( hasIcon )
		{
			iconTextSize = iconFont->CalcTextSizeA( drawIconSize , FLT_MAX , -1.f , iconText );
			if ( !icon && iconTextSize.x > 22.f )
			{
				drawIconSize = std::max( 8.f , iconFontSize * 22.f / iconTextSize.x );
				iconTextSize = iconFont->CalcTextSizeA( drawIconSize , FLT_MAX , -1.f , iconText );
			}
			iconW = std::max( iconBase , iconTextSize.x + 6.f );
		}

		// 名牌尺寸:图标槽 + 分隔线 + 垂直堆叠的名字(带距离)
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

		const float cardW = iconW + 6.f + maxTextW + padX * 2.f + 8.f;
		const float cardH = std::max( iconH , totalH ) + padY * 2.f;

		const float left = screen.x - cardW * 0.5f;
		const float top = screen.y - cardH - 18.f;
		const float iconLeft = left + padX;
		const float iconTop = top + ( cardH - iconH ) * 0.5f;
		const float dividerX = iconLeft + iconW + 3.f;

		const ImU32 base = AccentColor( 255 );
		drawList->AddRectFilled( { left , top } , { left + cardW , top + cardH } , IM_COL32( 0 , 0 , 0 , 150 ) , 4.f );

		if ( hasIcon )
		{
			drawList->AddText( iconFont , drawIconSize ,
				{ iconLeft + ( iconW - iconTextSize.x ) * 0.5f , iconTop + ( iconH - iconTextSize.y ) * 0.5f } ,
				base , iconText );
		}
		else
		{
			drawList->AddRectFilled( { iconLeft , iconTop } , { iconLeft + iconW , iconTop + iconH } , base , 2.f );
		}

		// 分隔线
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
