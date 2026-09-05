#include "CAimLock.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>
#include <random>

#include <PluginSenseClient/Settings/MenuState.hpp>
#include <PluginSenseClient/CPluginSenseGUI.hpp>
#include <PluginSenseClient/GUI/framework_w/includes.hh> // key_var_t(完整框架链,对齐 CHelper.cpp)

#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/FunctionListSDK.hpp>
#include <CS2/SDK/Interface/CGameEntitySystem.hpp>
#include <CS2/SDK/Interface/IEngineCvar.hpp>
#include <CS2/SDK/Types/CEntityData.hpp>
#include <CS2/SDK/Math/Math.hpp>

#include <GameClient/CL_Players.hpp>

// aimbot 热键 extern 见 CAimLock.hpp(与 CHelper 的 helper key 同模式,定义在 menu.cc)

namespace
{
	constexpr double kRad2Deg = 180.0 / 3.14159265358979323846;

	// 部位 → CS2 骨骼索引(对齐 vesta joint_id)
	// 顺序必须与菜单下拉一致:Head / Neck / Chest / Pelvis / Feet
	// 注意:catalyst 的 head=6 是 CS:GO 遗留编号,在 CS2 里 6 是 neck,真头骨是 7
	constexpr int kPartBones[ 5 ]{ 7 , 6 , 5 , 1 , 19 };
	constexpr int kPartCount = 5;

	float WrapYaw( float yaw )
	{
		yaw = std::fmod( yaw + 180.f , 360.f );
		if ( yaw < 0.f )
			yaw += 360.f;
		return yaw - 180.f;
	}

	float RandFloat( float min , float max )
	{
		static thread_local std::mt19937 rng{ std::random_device{}() };
		std::uniform_real_distribution<float> dist( min , max );
		return dist( rng );
	}

	// 正态分布 + 截断(人性化抖动用,对齐 catalyst random::normal_clamped)
	float NormalClamped( float mean , float stddev , float lo , float hi )
	{
		static thread_local std::mt19937 rng{ std::random_device{}() };
		std::normal_distribution<float> dist( mean , stddev );
		return std::clamp( dist( rng ) , lo , hi );
	}

	// 原生注入(NtUserInjectMouseInput),与 CHelper 同一机制
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

	bool GameHasInputFocus()
	{
		const auto foreground = ::GetForegroundWindow();
		const auto root = foreground ? ::GetAncestor( foreground , GA_ROOT ) : nullptr;
		DWORD processId{};
		if ( root )
			::GetWindowThreadProcessId( root , &processId );
		return processId != 0 && processId == ::GetCurrentProcessId();
	}
}

static CAimLock g_CAimLock{};

// ============================================================================
// CreateMove 钩子:只缓存输入指针(只读),对齐 CHelper
// ============================================================================
auto CAimLock::OnCreateMove( CCSGOInput* pInput ) -> void
{
	m_pInput = pInput;
}

// ============================================================================
// 每渲染帧:aimlock 主循环
// ============================================================================
auto CAimLock::OnRender( ImDrawList* drawList , int screenW , int screenH ) -> void
{
	const auto reset = [ this ]()
	{
		m_AimErrorX = m_AimErrorY = 0.f;
		m_pLockPawn = nullptr;   // 守卫/失活即清锁,保证下次按键重新获取
		m_LockLastSeen = std::chrono::steady_clock::time_point{};
	};

	if ( !drawList )
	{
		reset(); return;
	}

	// ---- 开关/菜单守卫(与按键无关:FOV 圈在 aimbot 开启时就显示)----
	if ( !menu_state::aimbotEnabled )
	{
		reset(); return;
	}

	// 菜单打开时不注入也不画圈(此时相对鼠标模式被关,注入会拖动真实光标)
	if ( GetPluginSenseGUI()->IsVisible() )
	{
		reset(); return;
	}

	auto* player = GetCL_Players()->GetLocalPlayerPawn();
	if ( !player || !player->IsAlive() || player->m_bIsBuyMenuOpen() )
	{
		reset(); return;
	}

	QAngle view{};
	if ( !GetViewAngles( view ) )
	{
		reset(); return;
	}

	// ---- FOV 圈可视化(画在屏幕中心,半径 = FOV 度数的屏幕投影)----
	if ( menu_state::aimbotDrawFov )
		DrawFovCircle( drawList , screenW , screenH , view );

	// ---- 瞄准守卫(需要按键)----
	if ( !aimbot::g_aimbot_key.active() )
	{
		reset(); return;
	}

	if ( !GameHasInputFocus() )
	{
		reset(); return;
	}

	const Vector3 eye = GetCL_Players()->GetLocalEyeOrigin();

	const int lockTimeMs = menu_state::aimbotLockTime;
	const float maxFov = static_cast<float>( menu_state::aimbotFov );
	const bool infiniteLock = lockTimeMs <= 0; // 0ms = 无限锁:锁上后不因时间松手

	Target tgt{};
	bool haveTgt = false;

	if ( m_pLockPawn )
	{
		const auto now = std::chrono::steady_clock::now();

		// ---- 已有锁定目标:确认他现在是否仍在 FOV 内 ----
		bool seen = false;
		__try
		{
			Target locked{};
			if ( BuildTarget( m_pLockPawn , eye , view , locked ) && locked.fov <= maxFov )
			{
				tgt = locked;
				seen = true;
				m_LockLastSeen = now; // 看到即刷新"最后看见"时刻
			}
		}
		__except ( EXCEPTION_EXECUTE_HANDLER )
		{
			seen = false; // 锁定目标内存异常,视为当前不可见
		}

		if ( seen )
		{
			if ( !infiniteLock && now >= m_LockExpireAt )
			{
				// ---- 锁定期已满(过热停手):不瞄锁定目标 ----
				// 只有"最佳目标已换成别人"才切过去并重新计时;仍是最佳则保持停手,
				// 直到它失焦(离开 FOV/死亡)后再回来才重新获得(避免同一目标自动续)。
				Target best{};
				if ( FindBestTarget( eye , view , best ) && best.pawn != m_pLockPawn )
				{
					m_pLockPawn = best.pawn;
					m_LockLastSeen = now; // 新目标即视为已看到
					m_LockExpireAt = now + std::chrono::milliseconds( lockTimeMs );
					tgt = best;
					haveTgt = true;
				}
				else
				{
					// 仍只有锁定目标可选:停手但保留锁定(等它失焦才可能重新获得)
					haveTgt = false;
				}
			}
			else
			{
				// ---- 锁定中(未满 / 无限锁):专注锁定目标,不参与 FOV 竞争 ----
				haveTgt = true;
			}
		}
		else
		{
			// ---- 目标当前不可见(离开 FOV / 死亡):失焦宽限 400ms(对齐 vesta)----
			// 宽限内保留锁定:他 ≤400ms 内回来 = 仍是同一目标,锁定状态延续;
			// 死亡或超过宽限仍未看到才真正释放,之后重新按 FOV 获得。
			bool lost = false;
			__try
			{
				lost = !m_pLockPawn->IsAlive(); // 死亡 = 立即放弃,不等宽限
			}
			__except ( EXCEPTION_EXECUTE_HANDLER )
			{
				lost = true;
			}
			if ( lost || now - m_LockLastSeen > std::chrono::milliseconds( 400 ) )
				m_pLockPawn = nullptr;
		}
	}

	if ( !haveTgt && !m_pLockPawn )
	{
		// ---- 无锁定:全量按 FOV 选最优,选中者即作为锁定起点 ----
		haveTgt = FindBestTarget( eye , view , tgt );
		if ( haveTgt )
		{
			m_pLockPawn = tgt.pawn;
			m_LockLastSeen = std::chrono::steady_clock::now(); // 刚获得 = 刚看见
			m_LockExpireAt = std::chrono::steady_clock::now() + std::chrono::milliseconds( lockTimeMs );
		}
	}

	if ( !haveTgt )
	{
		if ( m_pLockPawn )
		{
			// 过热停手 / 失焦宽限:保留锁定(等他回来或超时),只清残差,不注入
			m_AimErrorX = m_AimErrorY = 0.f;
		}
		else
		{
			reset(); // 真无目标:清锁 + 残差
		}
		return;
	}

	AimAt( eye , view , tgt );
}

// ============================================================================
// 视角来源(对齐项目:CreateMove 钩子传 CCSGOInput,用 CCSGOInput_GetViewAngles)
// ============================================================================
bool CAimLock::GetViewAngles( QAngle& out ) const
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
// 单实体目标评估(FindBestTarget 扫描与锁定续用共用):
//   传入敌人 pawn → 存活/敌对检查 → 按勾选部位遍历 → 预判 → 返回 FOV 最小的
//   部位作为 aim 点。不判 FOV 上限,由调用方决定(全量竞争 / 锁定 FOV 校验)。
// ============================================================================
bool CAimLock::BuildTarget( C_CSPlayerPawn* pawn , const Vector3& eye , const QAngle& view , Target& out ) const
{
	if ( !pawn )
		return false;

	auto* localPawn = GetCL_Players()->GetLocalPlayerPawn();
	if ( !localPawn || pawn == localPawn )
		return false;
	if ( !pawn->IsAlive() )
		return false;

	const auto localTeam = localPawn->m_iTeamNum();
	const auto team = pawn->m_iTeamNum();
	if ( team == 0 || team == localTeam )
		return false;

	auto* sceneNode = pawn->m_pGameSceneNode();
	if ( !sceneNode )
		return false;

	// 勾选的部位 → 合法骨骼列表(顺序与菜单/kPartBones 一致)
	// 多部位勾选 = 智能模式:准心离哪个部位近就瞄哪个(取该敌人 FOV 最小的部位)
	// 覆盖键激活时用覆盖配置"替换"正常配置;覆盖配置为空 = 严格不瞄
	const bool overrideActive = aimbot::g_override_key.active();

	const bool partSelected[ kPartCount ]
	{
		overrideActive ? menu_state::aimbotOverridePartHead  : menu_state::aimbotPartHead ,
		overrideActive ? menu_state::aimbotOverridePartNeck  : menu_state::aimbotPartNeck ,
		overrideActive ? menu_state::aimbotOverridePartChest : menu_state::aimbotPartChest ,
		overrideActive ? menu_state::aimbotOverridePartPelvis: menu_state::aimbotPartPelvis ,
		overrideActive ? menu_state::aimbotOverridePartFeet  : menu_state::aimbotPartFeet
	};
	int eligibleBones[ kPartCount ]{};
	int eligibleCount = 0;
	for ( auto i = 0; i < kPartCount; ++i )
		if ( partSelected[ i ] )
			eligibleBones[ eligibleCount++ ] = kPartBones[ i ];

	if ( eligibleCount == 0 )
		return false; // 无任何勾选部位(含覆盖配置全空 = 严格不瞄)

	const float predictionTime = menu_state::aimbotPredictive ? GetPredictionTime() : 0.f;

	// 遍历勾选的部位,取该敌人 FOV 最小的部位作为瞄准点
	bool found = false;
	float bestFov = FLT_MAX;
	for ( auto bi = 0; bi < eligibleCount; ++bi )
	{
		Vector3 bonePos{};
		if ( !sceneNode->GetBonePosition( eligibleBones[ bi ] , bonePos ) )
			continue;
		if ( !std::isfinite( bonePos.m_x ) || !std::isfinite( bonePos.m_y ) || !std::isfinite( bonePos.m_z ) )
			continue;

		// 预判:aim_point + velocity * prediction_time(速度外推,对齐 catalyst)
		Vector3 aim = bonePos;
		if ( predictionTime > 0.f )
			aim = aim + pawn->m_vecAbsVelocity() * predictionTime;

		const Vector3 delta = aim - eye;
		const float dist2d = delta.Length2D();
		if ( dist2d < 0.001f )
			continue;

		// 角度差(与 AimAt 同口径)
		const float pitch = static_cast<float>( std::atan2f( -delta.m_z , dist2d ) * kRad2Deg );
		const float yaw = static_cast<float>( std::atan2f( delta.m_y , delta.m_x ) * kRad2Deg );
		const float dx = pitch - view.m_x;
		const float dy = WrapYaw( yaw - view.m_y );
		const float fov = std::sqrtf( dx * dx + dy * dy );

		if ( fov > bestFov )
			continue;

		bestFov = fov;
		out.pawn = pawn;
		out.aim_point = aim;
		out.fov = fov;
		found = true;
	}

	return found;
}

// ============================================================================
// 目标选择:遍历实体 → BuildTarget 逐实体评估 → FOV 筛选(取全局最小)
//
// 实体遍历(vesta 方案):签名解析 dwEntityList 全局,直接扫 4×512 chunk 数组,
// 不再依赖 GetHighestEntityIndex 硬编码偏移(旧偏移 0x2090 读不到真实索引)。
// 若 dwEntityList 解析失败,回退 GetBaseEntity 固定范围(函数自带越界检查)。
// ============================================================================
bool CAimLock::FindBestTarget( const Vector3& eye , const QAngle& view , Target& out ) const
{
	const float maxFov = static_cast<float>( menu_state::aimbotFov );

	bool found = false;
	float bestFov = maxFov;

	const auto consider = [ & ]( C_BaseEntity* entity )
	{
		if ( !entity || !entity->IsPlayerPawn() )
			return;

		auto* pawn = reinterpret_cast<C_CSPlayerPawn*>( entity );

		Target cand{};
		if ( !BuildTarget( pawn , eye , view , cand ) )
			return;
		if ( cand.fov > bestFov )
			return;

		bestFov = cand.fov;
		out = cand;
		found = true;
	};

	void* entityList = nullptr;

	__try
	{
		// ---- 主路径:实体表 chunk 扫描 ----
		entityList = SDK::Pointers::EntityList();
		if ( entityList )
		{
			constexpr std::size_t k_entries_per_chunk{ 512 };
			constexpr std::size_t k_entry_stride{ 0x70 };

			std::uintptr_t chunks[ 4 ]{};
			std::memcpy( chunks ,
				reinterpret_cast<void*>( reinterpret_cast<std::uintptr_t>( entityList ) + 0x10 ) ,
				sizeof( chunks ) );

			for ( auto ci = 0; ci < 4; ++ci )
			{
				const auto chunk = chunks[ ci ];
				if ( !chunk || chunk < 0x10000 )
					continue;

				for ( auto ei = 0; ei < static_cast<int>( k_entries_per_chunk ); ++ei )
				{
					const auto ptr = *reinterpret_cast<std::uintptr_t*>(
						chunk + static_cast<std::uintptr_t>( ei ) * k_entry_stride );
					if ( !ptr || ptr < 0x10000 )
						continue;

					consider( reinterpret_cast<C_BaseEntity*>( ptr ) );
				}
			}
		}
		else
		{
			// ---- 回退:GetBaseEntity 固定范围(函数自带越界检查,空槽返回 null)----
			for ( auto idx = 0; idx < 2048; ++idx )
			{
				auto* entity = SDK::Interfaces::GameEntitySystem()->GetBaseEntity<C_BaseEntity>( idx );
				if ( !entity )
					continue;

				consider( entity );
			}
		}
	}
	__except ( EXCEPTION_EXECUTE_HANDLER )
	{
		// 实体内存异常,忽略本轮扫描
	}

	return found;
}

// ============================================================================
// 预判时间 = 半程 ping + cl_interp(对齐 catalyst get_prediction_time)
// ============================================================================
float CAimLock::GetPredictionTime() const
{
	float latency = 0.f;
	if ( auto* controller = GetCL_Players()->GetLocalPlayerController() )
	{
		const int ping = controller->m_iPing();
		if ( ping > 0 && ping < 5000 )
			latency = static_cast<float>( ping ) * 0.0005f; // ping/2,ms → s
	}

	// cl_interp 缓存(500ms,对齐 CHelper 的 sensitivity 缓存方式)
	static float s_interp = 0.03125f;
	static auto s_lastUpdate = std::chrono::steady_clock::time_point{};
	const auto now = std::chrono::steady_clock::now();
	if ( now - s_lastUpdate >= std::chrono::milliseconds( 500 ) )
	{
		s_lastUpdate = now;
		if ( auto* pCvar = SDK::Interfaces::EngineCvar() )
		{
			if ( auto* convar = pCvar->Find( "cl_interp" ) )
			{
				if ( convar->nType == EConVarType_Float32 )
					s_interp = convar->value.fl;
			}
		}
	}

	return latency + s_interp;
}

// ============================================================================
// FOV 圈可视化(catalyst/vesta 同款算法):
//   投影"视角方向 1000u"与"视角上偏 fov 度方向 1000u"两个点,
//   屏幕距离即为 FOV 角对应的屏幕半径,圈画在屏幕中心。
// ============================================================================
void CAimLock::DrawFovCircle( ImDrawList* drawList , int screenW , int screenH , const QAngle& view ) const
{
	if ( !drawList )
		return;

	const float fovDeg = static_cast<float>( menu_state::aimbotFov );
	if ( fovDeg <= 0.f )
		return;

	const Vector3 eye = GetCL_Players()->GetLocalEyeOrigin();

	QAngle boundary = view;
	boundary.m_x -= fovDeg; // 俯仰上偏 fov 度(对准正前方时,圈的半径 = fov 角的屏幕投影)

	Vector3 forward{}, offset{};
	Math::AngleVectors( view , forward );
	Math::AngleVectors( boundary , offset );

	ImVec2 center{}, edge{};
	if ( !Math::WorldToScreen( eye + forward * 1000.f , center ) )
		return;
	if ( !Math::WorldToScreen( eye + offset * 1000.f , edge ) )
		return;

	const float dx = edge.x - center.x;
	const float dy = edge.y - center.y;
	const float radius = std::sqrtf( dx * dx + dy * dy );
	if ( radius <= 0.5f )
		return;

	const float sx = static_cast<float>( screenW ) * 0.5f;
	const float sy = static_cast<float>( screenH ) * 0.5f;

	const auto& c = menu_state::aimbotFovColor;
	const ImU32 col = IM_COL32(
		static_cast<int>( c.x * 255.f ) , static_cast<int>( c.y * 255.f ) ,
		static_cast<int>( c.z * 255.f ) , static_cast<int>( c.w * 255.f ) );

	drawList->AddCircle( ImVec2( sx , sy ) , radius , col , 64 , 1.5f );
}

// ============================================================================
// 瞄准内核(移植 catalyst legit::aimbot):
//   角度差 → 人性化平滑 → 灵敏度换算 → 残差累积 → 注入相对鼠标
// ============================================================================
void CAimLock::AimAt( const Vector3& eye , const QAngle& view , const Target& tgt )
{
	constexpr float kMousemoveYaw{ 0.022f };

	// sensitivity 缓存(500ms)
	static float s_sensitivity = 2.5f;
	static auto s_lastUpdate = std::chrono::steady_clock::time_point{};
	const auto now = std::chrono::steady_clock::now();
	if ( now - s_lastUpdate >= std::chrono::milliseconds( 500 ) )
	{
		s_lastUpdate = now;
		if ( auto* pCvar = SDK::Interfaces::EngineCvar() )
		{
			if ( auto* convar = pCvar->Find( "sensitivity" ) )
			{
				if ( convar->nType == EConVarType_Float32 )
					s_sensitivity = convar->value.fl;
			}
		}
	}

	float fovAdjust = 1.f;
	if ( auto* player = GetCL_Players()->GetLocalPlayerPawn() )
		fovAdjust = player->m_flFOVSensitivityAdjust();

	const float degPerPixel = s_sensitivity * kMousemoveYaw * fovAdjust;
	if ( degPerPixel <= 0.f )
		return;

	// 期望角度
	const Vector3 delta = tgt.aim_point - eye;
	const float dist2d = delta.Length2D();
	if ( dist2d < 0.001f )
	{
		m_AimErrorX = m_AimErrorY = 0.f;
		return;
	}

	const float pitch = static_cast<float>( std::atan2f( -delta.m_z , dist2d ) * kRad2Deg );
	const float yaw = static_cast<float>( std::atan2f( delta.m_y , delta.m_x ) * kRad2Deg );

	float deltaX = pitch - view.m_x;
	float deltaY = WrapYaw( yaw - view.m_y );
	const float deltaLength = std::sqrtf( deltaX * deltaX + deltaY * deltaY );

	if ( deltaLength < 0.001f )
	{
		m_AimErrorX = m_AimErrorY = 0.f;
		return;
	}

	// ---- 人性化平滑(对齐 catalyst:smoothing > 1 才平滑,0/1 = 直接锁)----
	if ( menu_state::aimbotSmoothing > 1 )
	{
		const float baseSmooth = static_cast<float>( menu_state::aimbotSmoothing );
		const float distanceFactor = std::clamp( deltaLength / 10.f , 0.f , 1.f );
		const float ease = 1.f - distanceFactor * distanceFactor;
		float smoothFactor = ( 0.3f + ease * 0.7f ) / baseSmooth;

		smoothFactor *= NormalClamped( 1.f , 0.06f , 0.85f , 1.15f );

		const float xBias = NormalClamped( 1.f , 0.02f , 0.95f , 1.05f );
		const float yBias = NormalClamped( 0.97f , 0.03f , 0.90f , 1.04f );

		deltaX *= smoothFactor * xBias;
		deltaY *= smoothFactor * yBias;

		// 近距离随机过冲,模拟人类微调
		if ( deltaLength < 2.f && deltaLength > 0.3f && RandFloat( 0.f , 1.f ) < 0.15f )
		{
			const float overshoot = NormalClamped( 1.2f , 0.08f , 1.05f , 1.4f );
			deltaX *= overshoot;
			deltaY *= overshoot;
		}
	}

	// 角度 → 鼠标像素数(符号约定与 catalyst/CHelper 一致)
	const float wantX = -deltaY + m_AimErrorX;
	const float wantY = deltaX + m_AimErrorY;

	const float countsX = std::roundf( wantX / degPerPixel );
	const float countsY = std::roundf( wantY / degPerPixel );

	// 残差累积:舍掉的余数留到下一帧,保证移动连续无停顿
	m_AimErrorX = wantX - countsX * degPerPixel;
	m_AimErrorY = wantY - countsY * degPerPixel;

	const int dx = static_cast<int>( countsX );
	const int dy = static_cast<int>( countsY );

	if ( dx != 0 || dy != 0 )
		InjectMouse( dx , dy , MOUSEEVENTF_MOVE );
}

auto GetAimLock() -> CAimLock*
{
	return &g_CAimLock;
}
