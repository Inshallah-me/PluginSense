#include "CMotionCamera.hpp"

#include <cmath>

#include <Common/Common.hpp>
#include <ImGui/imgui.h>
#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/FunctionListSDK.hpp>
#include <CS2/SDK/Math/Math.hpp>
#include <CS2/SDK/Math/Vector3.hpp>
#include <CS2/SDK/Math/QAngle.hpp>
#include <CS2/SDK/Types/CEntityData.hpp>
#include <CS2/SDK/Update/GameTrace.hpp>
#include <GameClient/CL_Players.hpp>
#include <PluginSenseClient/Settings/MenuState.hpp>

constexpr int kViewOrigin = 0x4A0;

static CMotionCamera g_MotionCamera{};

void CMotionCamera::Init()
{
}

void CMotionCamera::on_create_move( class CCSGOInput* pInput )
{
	if ( !pInput )
		return;
	const QAngle* angles = CCSGOInput_GetViewAngles( pInput, 0 );
	if ( angles )
	{
		m_view_pitch = angles->m_x;
		m_view_yaw = angles->m_y;
	}

	if ( !menu_state::worldScene.camCrosshair )
		return;

	auto* pPawn = GetCL_Players()->GetLocalPlayerPawn();
	if ( !pPawn || !pPawn->IsAlive() )
	{
		m_cross_valid = false;
		return;
	}

	const Vector3 eye = GetCL_Players()->GetLocalEyeOrigin();
	QAngle aim_angles( m_view_pitch, m_view_yaw, 0.f );

	Vector3 forward, dummy_r, dummy_u;
	Math::AngleVectors( aim_angles, forward, dummy_r, dummy_u );

	const Vector3 trace_end = eye + forward * 32768.f;

	Ray_t ray;
	CGameTrace trace;
	CTraceFilter filter( 0x1C1003, pPawn, 3, 15 );

	Vector3 hit_pos = trace_end;

	if ( IGamePhysicsQuery_TraceShape( SDK::Pointers::CVPhys2World(), ray, eye, trace_end, &filter, &trace ) )
	{
	hit_pos = trace.vecEnd;
	}

	ImVec2 screen;
	if ( Math::WorldToScreen( hit_pos, screen ) )
	{
		m_cross_x = screen.x;
		m_cross_y = screen.y;
		m_cross_valid = true;
	}
	else
	{
		m_cross_valid = false;
	}
}

void CMotionCamera::on_override_view( std::uintptr_t view_setup )
{
	const auto& sc = menu_state::worldScene;
	if ( !sc.motionCamera )
		return;

	// 死亡(含观战别人)时运动相机不生效,并重置状态避免残留。
	auto* pPawn = GetCL_Players()->GetLocalPlayerPawn();
	if ( !pPawn || !pPawn->IsAlive() )
	{
		m_initialized = false;
		m_is_thirdperson = false;
		return;
	}

	Vector3 eye = GetCL_Players()->GetLocalEyeOrigin();
	float* o = (float*)( view_setup + kViewOrigin );
	float dx = o[0] - eye.m_x, dy = o[1] - eye.m_y, dz = o[2] - eye.m_z;
	float dist = sqrtf( dx * dx + dy * dy + dz * dz );
	if ( dist < 32.f )
	{
		m_initialized = false;
		m_is_thirdperson = false;
		return;
	}

	m_is_thirdperson = true;

	const QAngle view_angles( m_view_pitch, m_view_yaw, 0.f );

	if ( !m_initialized )
	{
		m_cam_x = o[0]; m_cam_y = o[1]; m_cam_z = o[2];
		m_initialized = true;
	}
	else
	{
		const float slack = sc.camSlack * 0.001f;
		m_cam_x += ( o[0] - m_cam_x ) * slack;
		m_cam_y += ( o[1] - m_cam_y ) * slack;
		m_cam_z += ( o[2] - m_cam_z ) * slack;
	}

	Vector3 forward, right, up;
	Math::AngleVectors( view_angles, forward, right, up );

	o[0] = m_cam_x + right.m_x * sc.camHorOffset + up.m_x * sc.camVerOffset;
	o[1] = m_cam_y + right.m_y * sc.camHorOffset + up.m_y * sc.camVerOffset;
	o[2] = m_cam_z + right.m_z * sc.camHorOffset + up.m_z * sc.camVerOffset;
}

void CMotionCamera::on_render( ImDrawList* drawList, int screenW, int screenH )
{
	const auto& sc = menu_state::worldScene;
	if ( !sc.motionCamera || !sc.camCrosshair || !m_is_thirdperson || !drawList )
		return;
	if ( !m_cross_valid )
		return;

	const float gap = 3.f;
	const float len = sc.camCrosshairLength;
	const float thick = sc.camCrosshairThickness;
	const float cx = m_cross_x;
	const float cy = m_cross_y;
	const ImU32 clr = ImGui::ColorConvertFloat4ToU32( sc.camCrosshairColor );
	const float d = len * 0.7071f; // diagonal component

	// Top-left to bottom-right diagonal, gap in center
	drawList->AddLine( ImVec2( cx - gap - d, cy - gap - d ), ImVec2( cx - gap, cy - gap ), clr, thick );
	drawList->AddLine( ImVec2( cx + gap, cy + gap ), ImVec2( cx + gap + d, cy + gap + d ), clr, thick );

	// Top-right to bottom-left diagonal, gap in center
	drawList->AddLine( ImVec2( cx + gap + d, cy - gap - d ), ImVec2( cx + gap, cy - gap ), clr, thick );
	drawList->AddLine( ImVec2( cx - gap, cy + gap ), ImVec2( cx - gap - d, cy + gap + d ), clr, thick );
}

auto GetMotionCamera() -> CMotionCamera*
{
	return &g_MotionCamera;
}
