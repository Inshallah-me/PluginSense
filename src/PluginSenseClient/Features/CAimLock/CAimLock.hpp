#pragma once

#include <Common/Common.hpp>

#include <chrono>

#include <CS2/SDK/Math/Vector3.hpp>
#include <CS2/SDK/Math/QAngle.hpp>

class C_CSPlayerPawn;
class CCSGOInput;
struct ImDrawList;

namespace framework
{
	struct key_var_t;
}

namespace aimbot
{
	// 主键:默认不绑(避免误触),用户在 palette/Keybinds 里绑定;覆盖键:默认不绑 + Toggle
	extern framework::key_var_t g_aimbot_key;
	extern framework::key_var_t g_override_key;
}

// ============================================================================
// Aimbot(aimlock)
// 移植自 catalyst 的 legit aimbot 核心:
//   - 目标选择:遍历敌人,锁头(或胸),FOV 筛选
//   - 预判:aim_point + velocity * (ping/2 + cl_interp)
//   - 写入:注入相对鼠标移动(NtUserInjectMouseInput),灵敏度换算 + 残差累积,
//     与 CHelper::AimAt 同一套机制
// 第一版范围:锁头 + 预判,无可见性/穿墙检查。
// ============================================================================
class CAimLock final
{
public:
	// CreateMove 钩子传入 CCSGOInput(只缓存指针,不写任何东西,对齐 CHelper)
	auto OnCreateMove( CCSGOInput* pInput ) -> void;

	// 每渲染帧驱动:守卫 → 选目标 → 注入(对齐 CHelper 的 OnRender → Tick 节奏)
	auto OnRender( ImDrawList* drawList , int screenW , int screenH ) -> void;

private:
	struct Target
	{
		C_CSPlayerPawn* pawn = nullptr;
		Vector3 aim_point{};
		float fov = 0.f;
	};

	// 读当前视角(来自缓存 CCSGOInput 的 m_vecViewAngles)
	bool GetViewAngles( QAngle& out ) const;

	// 只对单个实体评估:敌人/存活/部位遍历 → 预判 → 取 FOV 最小的部位(不判 FOV 上限,
	// 由调用方决定)。FindBestTarget 全量扫描与换人延迟的"锁定续用"共用。
	bool BuildTarget( C_CSPlayerPawn* pawn , const Vector3& eye , const QAngle& view , Target& out ) const;

	// 遍历实体,选 FOV 最小且 <= 配置 FOV 的敌人
	bool FindBestTarget( const Vector3& eye , const QAngle& view , Target& out ) const;

	// 预判时间 = 半程 ping + cl_interp
	float GetPredictionTime() const;

	// 瞄准内核:角度差 → 平滑 → 灵敏度换算 → 残差累积 → 注入鼠标
	void AimAt( const Vector3& eye , const QAngle& view , const Target& tgt );

	// FOV 圈可视化:投影视角中心/上偏 fov 度的点,算屏幕半径,画在屏幕中心
	void DrawFovCircle( ImDrawList* drawList , int screenW , int screenH , const QAngle& view ) const;

private:
	CCSGOInput* m_pInput = nullptr;

	// 亚像素残差(跨帧累积,保证鼠标移动连续,对齐 catalyst m_aim_error)
	float m_AimErrorX = 0.f;
	float m_AimErrorY = 0.f;

	// 目标锁定:当前锁定的敌人 + 本窗口到期时刻(aimbotLockTime > 0 时启用)。
	// 锁上后专注该目标;到期(过热)即停手不自动续,只有"换到另一个人"或
	// "目标失焦(离开 FOV/死亡)后重新获得"才重新计时。0ms = 无限锁:不因时间松手,
	// 直到目标失效/松键。目标失效/松键即清空。
	//
	// 失焦宽限(对齐 vesta):目标离开 FOV 后不清锁,保留 m_LockLastSeen;
	// 若在宽限内(400ms)回来 = 仍是同一目标,锁定状态延续;
	// 超过宽限仍未看到才真正清锁,之后重新按 FOV 获得。
	C_CSPlayerPawn* m_pLockPawn = nullptr;
	std::chrono::steady_clock::time_point m_LockExpireAt{};
	std::chrono::steady_clock::time_point m_LockLastSeen{};
};

auto GetAimLock() -> CAimLock*;
