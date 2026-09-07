#include "CHelper.hpp"
#include "CHelperDetail.hpp"

#include <cmath>
#include <algorithm>

#include <PluginSenseClient/Settings/MenuState.hpp>

#include <GameClient/CL_Players.hpp>

namespace nd = resources::nades;

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
