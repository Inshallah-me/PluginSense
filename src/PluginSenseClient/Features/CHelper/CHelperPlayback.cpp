#include "CHelper.hpp"
#include "CHelperDetail.hpp"

#include <cmath>
#include <algorithm>

#include <Common/DevLog.hpp>

#include <CS2/SDK/SDK.hpp>

#include <PluginSenseClient/Settings/MenuState.hpp>

#include <GameClient/CL_Players.hpp>

namespace nd = resources::nades;

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
