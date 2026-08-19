#pragma once

#include <Common/Common.hpp>

#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

#include <CS2/SDK/Math/Vector3.hpp>
#include <CS2/SDK/Math/QAngle.hpp>

#include "nade_data.hpp"

class C_CSPlayerPawn;
class CCSGOInput;
struct ImDrawList;

namespace framework
{
	struct key_var_t;
}

namespace helper
{
	extern framework::key_var_t g_helper_key;
	extern framework::key_var_t g_move_forward;
	extern framework::key_var_t g_move_back;
	extern framework::key_var_t g_move_left;
	extern framework::key_var_t g_move_right;
	extern framework::key_var_t g_move_walk;
	extern framework::key_var_t g_move_duck;
	extern framework::key_var_t g_move_jump;
	extern framework::key_var_t g_attack_key;
	extern framework::key_var_t g_attack2_key;
}

// 输入设备与绑定
enum class InputDevice : std::uint8_t
{
	Keyboard,
	MousePrimary,
	MouseSecondary,
	MouseMiddle,
	MouseAux1,
	MouseAux2,
};

struct InputBinding
{
	InputDevice device = InputDevice::Keyboard;
	int virtualKey = 0;

	// 绑定有效 = 非"Keyboard + 无键"。不强制必须是鼠标左/右键,
	// 玩家自定义的 attack 键(如右键/键盘键)也能正常注入。
	explicit operator bool() const
	{
		return device != InputDevice::Keyboard || virtualKey != 0;
	}
};

enum class InputAction : std::uint8_t
{
	Forward,
	Back,
	Left,
	Right,
	Walk,
	Duck,
	Jump,
	Attack,
	Attack2,
};

struct OwnedControl
{
	bool pressed = false;
	InputBinding binding{};
};

// 可见点位
struct LineupView
{
	const char* name = nullptr;
	const char* action = nullptr;
	Vector3 position{};
	float pitch = 0.f;
	float yaw = 0.f;
	std::uint8_t kind = 0;   // resources::nades::kind
	std::uint16_t actions = 0;
	std::uint16_t run_ticks = 0;
	std::uint8_t after_jump_ticks = 0;
	float throw_strength = 1.f;
	bool manual = false;
	float distance = 0.f;
};

class CHelper final
{
public:
	auto OnRender( ImDrawList* drawList , int screenW , int screenH ) -> void;
	auto Tick() -> void;
	// CreateMove 钩子传入 CCSGOInput(只缓存指针,不写任何东西)
	auto OnCreateMove( class CCSGOInput* pInput ) -> void;

private:
	// 读当前视角(优先用钩子里的 CCSGOInput,对齐项目 CCSGOInput_GetViewAngles)
	bool GetRenderCameraAngles( QAngle& out ) const;

	// 选择
	bool Collect( const Vector3& playerPos , std::uint8_t weaponKind , std::vector<LineupView>& out ) const;
	int SelectArmed( const std::vector<LineupView>& lineups , const QAngle& viewAngles ) const;
	bool ExecutionPositionReady( const LineupView& lineup , const Vector3& playerPos ) const;
	std::uint8_t ResolveWeaponKind() const;

	// 执行
	void AimAt( const LineupView& lineup , const QAngle& viewAngles , float& outError );
	bool BeginThrow( const LineupView& lineup , std::uintptr_t pawn , std::uintptr_t weapon , std::uint32_t tick , std::chrono::steady_clock::time_point now );
	bool PrimeThrow( std::uint32_t tick , std::chrono::steady_clock::time_point now );
	void DriveThrow( std::uintptr_t pawn , std::uintptr_t weapon , std::uint32_t tick , std::chrono::steady_clock::time_point now );
	void FinishThrow( std::uint32_t tick );
	void CancelThrow( bool latch );

	bool SetControl( OwnedControl& control , bool pressed );
	void ReleaseMovement( bool includeJump );
	void ReleaseAttacks();
	void ResetLock();

	InputBinding ResolveBinding( InputAction action ) const;

	// 渲染
	void DrawMarker( ImDrawList* drawList , const LineupView& lineup , bool selected , float yOffset = 0.f ) const;
	void DrawStandMarker( ImDrawList* drawList , const LineupView& lineup , bool standing ) const;
	void DrawMouseAimPoints( ImDrawList* drawList , int screenW , int screenH ) const;

private:
	std::vector<LineupView> m_RenderScratch;
	std::vector<LineupView> m_TickScratch;

	// CreateMove 钩子传入的 CCSGOInput(只读,不写,避免与其它项目冲突)
	CCSGOInput* m_pInput = nullptr;

	// 输入状态
	OwnedControl m_Forward{};
	OwnedControl m_Walk{};
	OwnedControl m_Duck{};
	OwnedControl m_Jump{};
	OwnedControl m_Attack{};
	OwnedControl m_Attack2{};

	// 投掷状态机
	enum class ThrowPhase : std::uint8_t { Idle, Crouching, Priming, Running, Jumping, Complete };
	ThrowPhase m_ThrowPhase = ThrowPhase::Idle;
	LineupView m_ActiveLineup{};
	std::uintptr_t m_ActivePawn = 0;
	std::uintptr_t m_ActiveWeapon = 0;
	std::uint32_t m_PhaseTick = 0;
	std::uint32_t m_RunStartTick = 0;
	std::uint32_t m_JumpTick = 0;
	std::chrono::steady_clock::time_point m_PhaseStarted{};
	bool m_ActivationLatched = false;

	// 瞄准
	float m_AimErrorX = 0.f;
	float m_AimErrorY = 0.f;
	std::chrono::steady_clock::time_point m_LastAimUpdate{};

	// 锁定
	std::chrono::steady_clock::time_point m_LockStarted{};
	const char* m_LockName = nullptr;
	Vector3 m_LockPosition{};
	float m_LockPitch = 0.f;
	float m_LockYaw = 0.f;
};

auto GetHelper() -> CHelper*;
