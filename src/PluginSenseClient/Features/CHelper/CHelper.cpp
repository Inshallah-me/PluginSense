#include "CHelper.hpp"
#include "CHelperDetail.hpp"
#include "CHelperRecorder.hpp"

#include <cmath>
#include <algorithm>
#include <cstdio>
#include <numbers>

#include <Common/DevLog.hpp>

#include <CS2/SDK/Update/CCSGOInput.hpp>
#include <CS2/SDK/Math/Math.hpp>

#include <PluginSenseClient/Settings/MenuState.hpp>
#include <PluginSenseClient/GUI/framework_w/includes.hh>

#include <GameClient/CL_Players.hpp>
#include <GameClient/CL_Weapons.hpp>

namespace nd = resources::nades;

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
// 地图与雷类型标注
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
// 收集与选择
// ============================================================================
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

	// ---- 时间线回放(点位库):激活期间独占执行路径 ----
	// 必须先于墙点分支:回放中切枪(含丢雷后自动切回)会让 kind 变为 wallbang,
	// 墙点分支一旦抢跑,回放的中止路径被短路,已注入的移动键永不释放(表现为一直往前走)
	if ( m_TimelineActive )
	{
		if ( kind != m_TimelineKind )
		{
			// 切枪/切雷/切刀(手动切换或丢雷后自动切回):中止并锁存,释放全部注入键
			CancelThrow( true );
			return;
		}
		UpdateTimelinePlayback();
		return;
	}

	// ---- 墙点(穿点):走位/动作复现,不投掷不开枪 ----
	if ( kind == static_cast<std::uint8_t>( nd::kind::wallbang ) )
	{
		// 锁存期(自动执行结束后、松开热键前)不重新走位
		if ( m_ActivationLatched )
			return;

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

auto GetHelper() -> CHelper*
{
	return &g_CHelper;
}
