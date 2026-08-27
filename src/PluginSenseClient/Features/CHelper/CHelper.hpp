#pragma once

#include <Common/Common.hpp>

#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

#include <CS2/SDK/Math/Vector3.hpp>
#include <CS2/SDK/Math/QAngle.hpp>

#include "nade_data.hpp"

struct UserLineup; // 前置声明(CHelperRecorder.hpp 定义,避免头文件互相包含)

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
	extern framework::key_var_t g_record_key;
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
	std::string name;
	std::string action;
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

	// Recorder 面板数据:当前地图已录制点位的显示标签(与 Recorder 列表索引对齐)。
	// kindFilter:0xff=全部,否则只列该雷类型(nd::kind)
	std::vector<std::string> BuildRecorderItems( std::uint8_t kindFilter = 0xff ) const;

	// 删除当前地图第 index 条录制点位(与 BuildRecorderItems 索引对齐)。
	bool RemoveRecorderItem( int index );
	// 清空当前地图的所有录制点位。
	void ClearRecorderMap();
	// 读取当前地图第 index 条录制点位(编辑用),成功返回 true。
	bool GetRecorderItem( int index , UserLineup& out ) const;
	// 覆盖保存当前地图第 index 条录制点位(编辑用),成功返回 true。
	bool UpdateRecorderItem( int index , const UserLineup& lineup );
	// 筛选后列表位置 → 用户点位原始索引(供编辑/删除定位),无匹配返回 -1。
	int GetRecorderIndexAt( int listPos , std::uint8_t kindFilter ) const;

	// ---- 内置点位编辑(方案 B:复制覆盖,内置源数据只读)----
	// 当前地图全部内置点位的列表标签(内置索引 + 名字 + 动作)
	// kindFilter:0xff=全部,否则只列该雷类型(nd::kind)
	std::vector<std::string> BuildBuiltinItems( std::uint8_t kindFilter = 0xff ) const;
	// 读取当前地图内置点位(编辑用),成功返回 true。
	// kindFilter:0xff=全部,否则只统计匹配该雷类型的项(listPos 为筛选后的位置)
	bool GetBuiltinItem( int listPos , std::uint8_t kindFilter , UserLineup& out ) const;
	// 保存对内置第 index 条的覆盖(用户表中有同索引条目则更新,否则新增)。
	bool SaveBuiltinOverride( int builtinIndex , const UserLineup& lineup );
	// 移除对内置第 index 条的覆盖(恢复内置原样),成功返回 true。
	bool RemoveBuiltinOverride( int builtinIndex );

	// 传送玩家到指定点位(控制台 setpos/setang,需 sv_cheats 1;仅传送不自动执行)
	void TeleportTo( const UserLineup& lineup );
	// 设置下次录制使用的点位名(空 = 自动 Custom N,由面板输入框写入)
	void SetRecordName( const std::string& name );

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

	// 自动走位:按 WASD 走向点位 + 到位反向刹车
	void DriveToPoint( const LineupView& lineup , const Vector3& playerPos , const QAngle& viewAngles );
	void SetBrakeKeys( bool on );

	InputBinding ResolveBinding( InputAction action ) const;

	// 渲染
	void DrawMarker( ImDrawList* drawList , const LineupView& lineup , bool selected , float yOffset = 0.f ) const;
	void DrawStandMarker( ImDrawList* drawList , const LineupView& lineup , bool standing ) const;
	void DrawMouseAimPoints( ImDrawList* drawList , int screenW , int screenH ) const;

private:
	std::vector<LineupView> m_RenderScratch;
	std::vector<LineupView> m_TickScratch;

	// ===== 录制会话(toggle 键:按下开始录制,再按一下保存)=====
	// 下次录制使用的点位名(空 = 自动 Custom N)
	std::string m_RecordName;
	// 边沿检测:上一帧 toggle 状态
	bool m_RecordKeyPrev = false;
	// 当前是否在录制会话中
	bool m_RecordSessionActive = false;
	// 会话已就绪(玩家已静止且手持投掷物后才开始正式记录)
	bool m_SessionReady = false;
	// 会话开始时间(状态卡片显示录制时长用)
	std::chrono::steady_clock::time_point m_SessionStartTime{};

	// 会话内跟踪:拔销 = 投掷动作开始,松手/雷离手 = 出手
	bool m_SessionPinArmed = false;
	bool m_SessionWasAirborne = false;
	bool m_SessionWasCrouch = false;
	bool m_SessionWasWalk = false;        // 跑动期间是否按过静步键(action_walk)
	bool m_SessionThrew = false;          // 会话内是否真的投掷了
	std::uint32_t m_SessionArmTick = 0;   // 拔销 tick
	std::uint32_t m_SessionJumpTick = 0;  // 起跳 tick
	std::uint32_t m_SessionThrowTick = 0; // 出手 tick
	std::uint32_t m_SessionLastTick = 0;  // 上一帧游戏 tick(run_ticks 去重用)
	std::uint32_t m_SessionRunTicks = 0;  // 拔销后前进 tick 数
	std::uint8_t m_SessionAfterJumpTicks = 0;
	Vector3 m_SessionArmPos{};            // 拔销瞬间站位
	QAngle m_SessionThrowAngles{};        // 出手瞬间视角
	float m_SessionStrength = 1.f;        // 出手瞬间力度
	std::uint8_t m_SessionKind = 0xff;    // 出手时的雷类型

	void BeginRecordSession();
	void EndRecordSession();
	void UpdateRecordSession();
	void SaveSessionLineup();

	// 录制状态卡片(游戏内 HUD):红色 REC + 已录制时长
	void DrawRecordStatus( ImDrawList* drawList , int screenW , int screenH ) const;

	// 当前规范化地图名(空 = 不在游戏内)
	std::string GetCurrentMapName() const;
	// 雷类型 -> 显示名(供 Recorder 面板列表)
	std::string KindLabel( std::uint8_t kind ) const;

	// CreateMove 钩子传入的 CCSGOInput(只读,不写,避免与其它项目冲突)
	CCSGOInput* m_pInput = nullptr;

	// 输入状态
	OwnedControl m_Forward{};
	OwnedControl m_Back{};
	OwnedControl m_Left{};
	OwnedControl m_Right{};
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

	// 自动走位刹车
	bool m_Braking = false;
	bool m_BrakeF = false;
	bool m_BrakeB = false;
	bool m_BrakeL = false;
	bool m_BrakeR = false;
	float m_BrakeSpeed = 0.f;
	std::chrono::steady_clock::time_point m_BrakeStart{};

	// 锁定
	std::chrono::steady_clock::time_point m_LockStarted{};
	std::string m_LockName;
	Vector3 m_LockPosition{};
	float m_LockPitch = 0.f;
	float m_LockYaw = 0.f;
};

auto GetHelper() -> CHelper*;
