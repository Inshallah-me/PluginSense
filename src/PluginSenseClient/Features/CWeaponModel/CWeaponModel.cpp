#include "CWeaponModel.hpp"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <filesystem>
#include <string>
#include <unordered_map>

#include <Common/Common.hpp>
#include <DllLauncher.hpp>

#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/FunctionListSDK.hpp>
#include <CS2/SDK/Econ/CEconItemDefinition.hpp>
#include <CS2/SDK/Econ/CEconItemSchema.hpp>
#include <CS2/SDK/Econ/CEconItemSystem.hpp>
#include <CS2/SDK/Interface/CLocalize.hpp>
#include <CS2/SDK/Interface/CSource2Client.hpp>
#include <CS2/SDK/Types/CEntityData.hpp>

#include <GameClient/CL_Players.hpp>
#include <GameClient/CL_Weapons.hpp>

#include <PluginSenseClient/Settings/MenuState.hpp>
#include <PluginSenseClient/GUI/framework_w/render/fonts/weapon_icon_map.hpp>

namespace fs = std::filesystem;

static CWeaponModel g_CWeaponModel{};

namespace
{
	// ------------------------------------------------------------------ 显示名(按当前游戏语言)
	static std::string FixupWeaponName( const std::string& in )
	{
		auto s = in;
		if ( s.rfind( "weapon_" , 0 ) == 0 )
			s.erase( 0 , 7 );

		const auto underscore = s.find( '_' );
		if ( underscore != std::string::npos )
		{
			std::string a = s.substr( 0 , underscore );
			std::string b = s.substr( underscore + 1 );
			std::string out;
			if ( !a.empty() ) { out += static_cast<char>( toupper( a[0] ) ); out += a.substr( 1 ); }
			out += ' ';
			if ( !b.empty() ) { out += static_cast<char>( toupper( b[0] ) ); out += b.substr( 1 ); }
			return out;
		}

		std::string out;
		if ( !s.empty() ) { out += static_cast<char>( toupper( s[0] ) ); out += s.substr( 1 ); }
		return out;
	}

	// m_pszItemBaseName 是本地化键(如 #SFUI_WPNHUD_AK47),走 CLocalize 解析成当前语言
	static std::string LocalizeWeaponName( const char* pszBaseName , const char* pszFallback )
	{
		if ( pszBaseName && pszBaseName[0] == '#' )
		{
			if ( auto* pLocalize = SDK::Interfaces::Localize() )
			{
				if ( const char* pszLocalized = pLocalize->FindSafe( pszBaseName ) )
				{
					if ( pszLocalized[0] && pszLocalized[0] != '#' )
						return pszLocalized;
				}
			}
		}

		return pszFallback ? FixupWeaponName( pszFallback ) : std::string();
	}

	// m_pszWeaponName 是短名(如 weapon_ak47),去掉 weapon_ 前缀得到图标映射 key(ak47)
	static std::string WeaponShortName( const char* pszWeaponName )
	{
		std::string s = pszWeaponName ? pszWeaponName : std::string();
		if ( s.rfind( "weapon_" , 0 ) == 0 )
			s.erase( 0 , 7 );
		return s;
	}

	// 武器排序权重:刀 -> 电击枪 -> 手枪 -> SMG -> 步枪 -> 狙击 -> 霰弹 -> 机枪(类内按价格从低到高)
	static const std::unordered_map<std::string , int> k_weapon_order =
	{
		{ "knife" , 0 } , { "taser" , 1 } ,
		{ "glock" , 2 } , { "hkp2000" , 3 } , { "usp_silencer" , 4 } , { "p250" , 5 } ,
		{ "elite" , 6 } , { "fiveseven" , 7 } , { "tec9" , 8 } , { "cz75a" , 9 } ,
		{ "revolver" , 10 } , { "deagle" , 11 } ,
		{ "mac10" , 12 } , { "ump45" , 13 } , { "mp9" , 14 } , { "bizon" , 15 } ,
		{ "mp5sd" , 16 } , { "mp7" , 17 } , { "p90" , 18 } ,
		{ "galilar" , 19 } , { "famas" , 20 } , { "ak47" , 21 } , { "m4a1_silencer" , 22 } ,
		{ "m4a1" , 23 } , { "sg556" , 24 } , { "aug" , 25 } ,
		{ "ssg08" , 26 } , { "awp" , 27 } , { "scar20" , 28 } , { "g3sg1" , 29 } ,
		{ "nova" , 30 } , { "sawedoff" , 31 } , { "mag7" , 32 } , { "xm1014" , 33 } ,
		{ "negev" , 34 } , { "m249" , 35 } ,
	};

	// ------------------------------------------------------------------ 模型资源预加载
	// 用 CResourceSystem::BlockingLoadResourceByName 加载模型(结构布局与脚本的 TypedResourceName 一致)。
	// 不缓存:每次应用前都重新加载,即使资源被 GC 也能在切回来时重新加载。
	struct resource_name_t
	{
		std::uint32_t nTotalCount{};
		std::uint32_t nAllocated{ 0xc00000c8 };
		union
		{
			std::uintptr_t pString;
			std::uint8_t szString[200]{};
		};
		std::uintptr_t nExtension{};
		std::uintptr_t nExtension2{};
	};

	static auto PrecacheModel( const std::string& path ) -> void
	{
		resource_name_t buffer{};
		InitParticlePathBuffer( &buffer , path.c_str() );
		buffer.nExtension2 = 'vmdl';

		if ( auto* pResourceSystem = SDK::Interfaces::ResourceSystem() )
			ResourceSystemLoad( pResourceSystem , &buffer , "" );
	}

	// ------------------------------------------------------------------ 默认模型路径(恢复原版用)
	static auto GetDefaultModelPath( C_BasePlayerWeapon* pWeapon ) -> const char*
	{
		auto* pAttr = pWeapon->m_AttributeManager();
		if ( !pAttr )
			return nullptr;

		auto* pItem = pAttr->m_Item();
		if ( !pItem )
			return nullptr;

		auto* pDef = pItem->GetStaticData();
		if ( !pDef )
			return nullptr;

		return pDef->m_pszModelName();
	}

	// ------------------------------------------------------------------ 枪械过滤
	static const char* const g_GunTypeNames[] =
	{
		"#CSGO_Type_Pistol",
		"#CSGO_Type_SMG",
		"#CSGO_Type_Rifle",
		"#CSGO_Type_Shotgun",
		"#CSGO_Type_Machinegun",
		"#CSGO_Type_SniperRifle",
	};

	static auto IsGunDefinition( CEconItemDefinition* pDef ) -> bool
	{
		const char* pszType = pDef->m_pszItemTypeName();
		if ( !pszType )
			return false;

		for ( auto* gunType : g_GunTypeNames )
		{
			if ( strcmp( pszType , gunType ) == 0 )
				return pDef->IsWeapon();
		}

		return false;
	}
}

auto CWeaponModel::Init() -> void
{
	m_bInitialized = false;
	m_entries.clear();
	m_models.clear();
}

auto CWeaponModel::Shutdown() -> void
{
	m_bInitialized = false;
	m_entries.clear();
	m_models.clear();
}

auto CWeaponModel::Refresh() -> void
{
	m_bInitialized = false;
	m_entries.clear();
	m_models.clear();
}

auto CWeaponModel::GetWeaponNames() -> std::vector<std::string>
{
	if ( !m_bInitialized )
	{
		CollectWeapons();
		ScanModels();
		m_bInitialized = true;
	}

	std::vector<std::string> names;
	names.reserve( m_entries.size() );

	for ( auto& entry : m_entries )
		names.push_back( entry.szName );

	return names;
}

auto CWeaponModel::GetWeaponIconChars() -> std::vector<std::string>
{
	if ( !m_bInitialized )
	{
		CollectWeapons();
		ScanModels();
		m_bInitialized = true;
	}

	std::vector<std::string> icons;
	icons.reserve( m_entries.size() );

	for ( auto& entry : m_entries )
	{
		const auto iconIt = weapon_icon_map::icon_table.find( entry.szKey );
		icons.push_back( iconIt != weapon_icon_map::icon_table.end() ? iconIt->second : std::string() );
	}

	return icons;
}

auto CWeaponModel::GetModelNames() -> std::vector<std::string>
{
	if ( !m_bInitialized )
	{
		CollectWeapons();
		ScanModels();
		m_bInitialized = true;
	}

	std::vector<std::string> names;
	names.reserve( m_models.size() );

	for ( auto& m : m_models )
		names.push_back( m.szDisplay );

	return names;
}

auto CWeaponModel::SyncWeaponToModel( int weaponSel , int& modelSel ) -> void
{
	if ( m_entries.empty() )
	{
		modelSel = 0;
		return;
	}

	weaponSel = std::clamp( weaponSel , 0 , static_cast<int>( m_entries.size() ) - 1 );
	const auto& path = m_entries[weaponSel].szPath;

	modelSel = 0; // default
	for ( int i = 1; i < static_cast<int>( m_models.size() ); i++ )
	{
		if ( m_models[i].szPath == path )
		{
			modelSel = i;
			break;
		}
	}
}

auto CWeaponModel::SyncModelToWeapon( int weaponSel , int modelSel ) -> void
{
	if ( m_entries.empty() )
		return;

	weaponSel = std::clamp( weaponSel , 0 , static_cast<int>( m_entries.size() ) - 1 );
	if ( modelSel < 0 || modelSel >= static_cast<int>( m_models.size() ) )
		return;

	m_entries[weaponSel].szPath = m_models[modelSel].szPath;
}

auto CWeaponModel::CollectWeapons() -> void
{
	m_entries.clear();

	// Knife(所有刀都归到这一条)
	m_entries.push_back( { "Knife" , "knife" , -1 , "" } );

	auto* pClient = SDK::Interfaces::Source2Client();
	if ( !pClient )
		return;

	auto* pSystem = pClient->GetEconItemSystem();
	if ( !pSystem )
		return;

	auto* pSchema = pSystem->GetEconItemSchema();
	if ( !pSchema )
		return;

	auto& map = pSchema->GetSortedItemDefinitionMap();

	for ( const auto& node : map )
	{
		auto* pDef = node.m_value;
		if ( !pDef )
			continue;

		// 电击枪的物品类型是 #CSGO_Type_Equipment(不在枪械类型名单),按短名单独放行
		const std::string shortName = WeaponShortName( pDef->m_pszWeaponName() );
		if ( !IsGunDefinition( pDef ) && shortName != "taser" )
			continue;

		weapon_entry_t entry;
		entry.nDefIndex = pDef->m_nDefIndex();
		entry.szKey = WeaponShortName( pDef->m_pszWeaponName() );
		entry.szName = LocalizeWeaponName( pDef->m_pszItemBaseName() , pDef->m_pszWeaponName() );
		m_entries.push_back( std::move( entry ) );
	}

	// 按权重表排序(刀 -> 电击枪 -> 手枪 -> SMG -> 步枪 -> 狙击 -> 霰弹 -> 机枪),
	// 不在表里的武器兜底放最后按名字排。
	if ( m_entries.size() > 1 )
	{
		std::sort( m_entries.begin() , m_entries.end() ,
			[]( const auto& a , const auto& b )
			{
				const auto oa = k_weapon_order.find( a.szKey );
				const auto ob = k_weapon_order.find( b.szKey );
				const int ia = oa != k_weapon_order.end() ? oa->second : 9999;
				const int ib = ob != k_weapon_order.end() ? ob->second : 9999;
				if ( ia != ib )
					return ia < ib;
				return a.szName < b.szName;
			} );
	}
}

auto CWeaponModel::ScanModels() -> void
{
	m_models.clear();
	m_models.push_back( { "default" , "" } );

	// GetCS2Dir() 返回 ...\game\bin\win64\;把 bin\win64 替换成 csgo\weapons
	std::string root = GetCS2Dir();
	if ( const auto pos = root.find( "bin\\win64" ); pos != std::string::npos )
		root.replace( pos , 9 , "csgo\\weapons\\" );
	else
		root += "..\\..\\csgo\\weapons\\"; // 兜底

	try
	{
		std::error_code ec;
		if ( !fs::exists( root , ec ) )
			return;

		for ( auto it = fs::recursive_directory_iterator( root , ec );
		      it != fs::recursive_directory_iterator(); it.increment( ec ) )
		{
			if ( ec )
				break;

			const auto& path = it->path();
			if ( path.extension() != ".vmdl_c" )
				continue;

			// 引擎资源路径(相对 csgo 目录)
			std::string full = path.string();
			auto pos = full.find( "weapons\\" );
			if ( pos == std::string::npos )
				pos = full.find( "weapons/" );
			if ( pos == std::string::npos )
				continue;

			std::string rel = full.substr( pos );
			std::replace( rel.begin() , rel.end() , '\\' , '/' );

			// 规范化:合并连续斜杠(否则 weapons//flux/... 会让资源系统无法识别类型)
			{
				std::string clean;
				char prev = 0;
				for ( char c : rel )
				{
					if ( c == '/' && prev == '/' )
						continue;
					clean += c;
					prev = c;
				}
				rel = std::move( clean );
			}

			if ( rel.size() > 7 && rel.compare( rel.size() - 7 , 7 , ".vmdl_c" ) == 0 )
				rel = rel.substr( 0 , rel.size() - 7 ) + ".vmdl";

			// 统一小写(和游戏资源系统 ToLowerFast 后的格式一致),
			// 否则 current(游戏小写名)!= path(原大小写),每帧重试刷屏
			std::transform( rel.begin() , rel.end() , rel.begin() ,
				[]( unsigned char c ) { return static_cast<char>( std::tolower( c ) ); } );

			// 显示名:只显示文件名(如 blue_tiger_talon_ag2.vmdl_c),不带路径,保留原大小写
			const std::string display = path.filename().string();

			m_models.push_back( { display , rel } );
		}
	}
	catch ( ... )
	{
	}

	// 按显示名排序,default 保持第一
	if ( m_models.size() > 1 )
	{
		std::sort( m_models.begin() + 1 , m_models.end() ,
			[]( const auto& a , const auto& b ) { return a.szDisplay < b.szDisplay; } );
	}
}

auto CWeaponModel::FindEntryForWeapon( C_BasePlayerWeapon* pWeapon ) -> weapon_entry_t*
{
	if ( !pWeapon )
		return nullptr;

	auto* pAttr = pWeapon->m_AttributeManager();
	if ( !pAttr )
		return nullptr;

	auto* pItem = pAttr->m_Item();
	if ( !pItem )
		return nullptr;

	auto* pVData = pItem->GetBasePlayerWeaponVData();
	if ( pVData && pVData->m_WeaponType().m_Type == CSWeaponType_t::WEAPONTYPE_KNIFE )
	{
		// 所有刀都归到 Knife
		for ( auto& entry : m_entries )
		{
			if ( entry.nDefIndex == -1 )
				return &entry;
		}

		return nullptr;
	}

	const int nIndex = pItem->m_iItemDefinitionIndex();

	for ( auto& entry : m_entries )
	{
		if ( entry.nDefIndex == nIndex )
			return &entry;
	}

	return nullptr;
}

auto CWeaponModel::GetWeaponModelName( C_BaseEntity* pEntity ) -> std::string
{
	if ( !pEntity )
		return "";

	auto* pSceneNode = pEntity->m_pGameSceneNode();
	if ( !pSceneNode )
		return "";

	auto* pSkeleton = pSceneNode->GetSkeletonInstance();
	if ( !pSkeleton )
		return "";

	const char* pszName = pSkeleton->m_modelState().m_ModelName().String();
	return pszName ? pszName : "";
}

auto CWeaponModel::SetModelWithCollision( C_BaseEntity* pEntity , const std::string& path ) -> void
{
	if ( !pEntity )
		return;

	// SetModel 会清空碰撞,先把碰撞盒读出来再写回
	auto* pCollision = pEntity->m_pCollision();
	Vector3 mins{};
	Vector3 maxs{};
	if ( pCollision )
	{
		mins = pCollision->m_vecMins();
		maxs = pCollision->m_vecMaxs();
	}

	reinterpret_cast<C_BaseModelEntity*>( pEntity )->SetModel( path.c_str() );

	if ( pCollision )
	{
		pCollision->m_vecMins() = mins;
		pCollision->m_vecMaxs() = maxs;
	}
}

auto CWeaponModel::ApplyToPawn( C_CSPlayerPawn* pPawn ) -> void
{
	auto* pServices = pPawn->m_pWeaponServices();
	if ( !pServices )
		return;

	auto* pActive = GetCL_Weapons()->GetLocalActiveWeapon();

	auto& myWeapons = pServices->m_hMyWeapons();
	for ( int i = 0; i < myWeapons.Count(); i++ )
	{
		auto* pWeapon = myWeapons[i].Get<C_BasePlayerWeapon>();
		if ( !pWeapon )
			continue;

		auto* pEntry = FindEntryForWeapon( pWeapon );
		if ( !pEntry )
			continue; // 未在列表中的武器(雷/C4/电击枪等)不处理

		const std::string& path = pEntry->szPath;
		const std::string current = GetWeaponModelName( pWeapon );

		if ( path.empty() )
		{
			// default:恢复原版模型
			const char* pszDefault = GetDefaultModelPath( pWeapon );
			if ( pszDefault && pszDefault[0] && current != pszDefault )
				SetModelWithCollision( pWeapon , pszDefault );
		}
		else
		{
			if ( current != path )
			{
				PrecacheModel( path );
				SetModelWithCollision( pWeapon , path );
			}
		}

		// 当前活动武器的 HUD 模型一起替换
		if ( pWeapon == pActive )
		{
			auto* pHudWeapon = pPawn->GetViewModel();
			if ( pHudWeapon )
			{
				const std::string hudCurrent = GetWeaponModelName( pHudWeapon );
				const char* pszTarget = path.empty() ? GetDefaultModelPath( pWeapon ) : path.c_str();

				if ( pszTarget && pszTarget[0] && hudCurrent != pszTarget )
				{
					if ( !path.empty() )
						PrecacheModel( path );
					SetModelWithCollision( pHudWeapon , pszTarget );
				}
			}
		}
	}
}

auto CWeaponModel::OnFrame() -> void
{
	if ( !menu_state::weaponModelEnabled )
		return;

	auto* pPawn = GetCL_Players()->GetLocalPlayerPawn();
	if ( !pPawn || !pPawn->IsAlive() )
		return;

	ApplyToPawn( pPawn );
}

auto GetWeaponModel() -> CWeaponModel*
{
	return &g_CWeaponModel;
}
