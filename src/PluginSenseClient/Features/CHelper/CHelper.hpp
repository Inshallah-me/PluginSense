#pragma once

#include <Common/Common.hpp>

#include <string>
#include <vector>
#include <chrono>
#include <cstdint>

#include <CS2/SDK/Math/Vector3.hpp>
#include <CS2/SDK/Math/QAngle.hpp>

#include "NadeKinds.hpp"
#include "HelperTimeline.hpp"

struct UserLineup; // 前置声明(CHelperRecorder.hpp 定义,避免头文件互相包含)

class C_CSPlayerPawn;
class CCSGOInput;
class CUserCmd;
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
	Vector3 position{};
	float pitch = 0.f;
	float yaw = 0.f;
	std::uint8_t kind = 0;   // resources::nades::kind
	float distance = 0.f;
	// 穿点(墙bang)专用:录制时手持武器短名,名牌显示该枪图标
	std::string weapon;
	// 墙点标注位(1=Crouch 4=Jump),名牌/描点显示用
	std::uint8_t annotations = 0;
	// 时间线帧(点位库 / 用户自录):非空 = 走时间线回放引擎
	const helper_timeline::Frame* frames = nullptr;
	std::size_t frameCount = 0;
};

class CHelper final
{
public:
	auto OnRender( ImDrawList* drawList , int screenW , int screenH ) -> void;
	auto Tick() -> void;
	// CreateMove 钩子传入 CCSGOInput + UserCmd(缓存指针 + 每 tick 驱动录制采样;只读,不写任何东西)
	auto OnCreateMove( CCSGOInput* pInput , CUserCmd* pUserCmd ) -> void;

	// Recorder 面板数据:当前地图已录制点位的显示标签(与 Recorder 列表索引对齐)。
	// kindFilter:0xff=全部,否则只列该雷类型(resources::nades::kind)
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

	// ---- 内置点位(点位库时间线,只读:列表/传送)----
	std::vector<std::string> BuildBuiltinItems( std::uint8_t kindFilter = 0xff ) const;
	bool GetBuiltinItem( int listPos , std::uint8_t kindFilter , UserLineup& out ) const;
	bool SaveBuiltinOverride( int builtinIndex , const UserLineup& lineup );
	bool RemoveBuiltinOverride( int builtinIndex );

	// 传送玩家到指定点位(控制台 setpos/setang,需 sv_cheats 1;仅传送不自动执行)
	void TeleportTo( const UserLineup& lineup );
	// 设置下次录制使用的点位名(空 = 自动 Custom N,由面板输入框写入)
	void SetRecordName( const std::string& name );

	// 卸载路径专用:释放当前注入按住的所有键并复位状态(CDllLauncher::OnDestroy 调用)
	auto OnUnload() -> void;

private:
	// 读当前视角(优先用钩子里的 CCSGOInput,对齐项目 CCSGOInput_GetViewAngles)
	bool GetRenderCameraAngles( QAngle& out ) const;

	// 选择
	bool Collect( const Vector3& playerPos , std::uint8_t weaponKind , std::vector<LineupView>& out ) const;
	int SelectArmed( const std::vector<LineupView>& lineups , const QAngle& viewAngles ) const;
	bool ExecutionPositionReady( const LineupView& lineup , const Vector3& playerPos ) const;
	std::uint8_t ResolveWeaponKind() const;

	// 穿点(wallbang):走到位 → 瞄准 → 就绪;开枪交给 rage/玩家
	void DriveWallbang( const Vector3& playerPos , const QAngle& viewAngles , std::uint32_t tick , std::chrono::steady_clock::time_point now );
	void ResetWallbangAction();
	std::string LineupIconChar( const LineupView& lineup ) const;

	// 执行
	void AimAt( const LineupView& lineup , const QAngle& viewAngles , float& outError );
	void CancelThrow( bool latch );

	bool SetControl( OwnedControl& control , bool pressed );
	void ReleaseMovement( bool includeJump );
	void ReleaseAttacks();
	void ResetLock();

	// 手雷轨迹 PiP 预览:从回放状态派生(锁定 或 回放中),Tick 开头统一调用
	void UpdateGrenadePreview();
	void WriteGrenadePreview( bool on );

	// 自动走位:按 WASD 走向点位 + 到位反向刹车
	void DriveToPoint( const LineupView& lineup , const Vector3& playerPos , const QAngle& viewAngles );
	void SetBrakeKeys( bool on );

	// 时间线回放(点位库/自录帧序列,外部注入输出)
	void StartTimelinePlayback( const helper_timeline::Frame* frames , std::size_t count ,
		const std::string& name , std::uint8_t kind );
	void CancelTimelinePlayback();
	void UpdateTimelinePlayback();
	void ApplyTimelineButtons( std::uint64_t target );
	OwnedControl* TimelineControl( std::uint64_t bit );

	InputBinding ResolveBinding( InputAction action ) const;

	// 渲染
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
	// 会话已就绪(玩家已静止且手持投掷物/枪后才开始正式采样)
	bool m_SessionReady = false;
	// 会话开始时间(状态卡片显示录制时长用)
	std::chrono::steady_clock::time_point m_SessionStartTime{};

	// 时间线采样(每 usercmd 一帧:按钮 + 视角 + 位置)
	std::vector<helper_timeline::Frame> m_SessionFrames;
	bool m_SessionSawAttack = false;      // 是否见过攻击按下(拉销)
	int m_SessionTail = -1;               // 出手(攻击松开)后的尾巴剩余帧;-1 = 未出手
	bool m_SessionTailDone = false;       // 尾巴采完,等渲染侧落盘
	Vector3 m_SessionArmPos{};            // 就绪瞬间站位(= 回放触发点 sp)
	QAngle m_SessionThrowAngles{};        // 就绪瞬间视角(= 回放触发视角 sv)
	std::uint8_t m_SessionKind = 0xff;    // 就绪时锁定的类型
	std::uint32_t m_SessionLastTick = 0;  // 采样去重(每 tick 一帧)

	void BeginRecordSession();
	void EndRecordSession();
	void UpdateRecordSession();
	void SaveSessionLineup();

	// 录制状态卡片(游戏内 HUD):红色 REC + 已录制时长
	void DrawRecordStatus( ImDrawList* drawList , int screenW , int screenH ) const;

public:
	// ---- 墙点武器多选(编辑面板与录制共用)----
	// 可穿墙枪械清单(短名/显示名),供编辑面板的武器多选框使用
	struct WallWeaponOption { const char* shortName; const char* display; };
	static const std::vector<WallWeaponOption>& WallWeaponOptions();
	// 把逗号分隔武器集合拆到"选项索引 → 勾选"数组
	static void ParseWallWeapons( const std::string& list , std::vector<bool>& selected );
	// 按勾选数组生成逗号分隔武器集合(空 = 任意)
	static std::string BuildWallWeapons( const std::vector<bool>& selected );
	// 与 WallWeaponOptions 对齐的 esp 图标字符(空串 = 无图标),编辑面板武器多选框行前图标
	static const std::vector<std::string>& WallWeaponIcons();

	// 当前规范化地图名(空 = 不在游戏内)
	std::string GetCurrentMapName() const;
	// 雷类型 -> 显示名(供 Recorder 面板列表)
	std::string KindLabel( std::uint8_t kind ) const;

	// CreateMove 钩子传入的 CCSGOInput(只读,不写,避免与其它项目冲突)
	CCSGOInput* m_pInput = nullptr;
	// CreateMove 钩子传入的 UserCmd(录制采样读按钮位;只读)
	CUserCmd* m_pCmd = nullptr;

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

	// 状态锁存:执行/回放结束后挂起,等热键松开再复位(防连触)
	bool m_ActivationLatched = false;

	// 瞄准
	float m_AimErrorX = 0.f;
	float m_AimErrorY = 0.f;
	std::chrono::steady_clock::time_point m_LastAimUpdate{};

	// 高速减速带:接近点位预测会冲圈时提前松键滑行减速
	bool m_Coasting = false;
	std::chrono::steady_clock::time_point m_CoastStart{};

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

	// 墙点执行状态(动作仅作点位标注,执行端只走位/瞄准,不注入任何动作键)
	enum class WallPhase : std::uint8_t { Idle, Ready };
	WallPhase m_WallPhase = WallPhase::Idle;

	// 预览延迟关闭:最后一次"应当激活"的时刻(条件不满足后仍保持 500ms)
	std::chrono::steady_clock::time_point m_PreviewLastActive{};

	// 时间线回放状态(内置库 / 自录,帧序列拷贝)
	bool m_TimelineActive = false;
	std::vector<helper_timeline::Frame> m_TimelineFrames;
	std::string m_TimelineName;
	std::uint8_t m_TimelineKind = 0xff;
	std::uint32_t m_TimelineStartTick = 0;
	bool m_TimelineStartTickSet = false;
	std::uint32_t m_TimelineLastTick = 0;
	std::uint64_t m_TimelineInjected = 0; // 当前已按下(注入)的按钮掩码
	int m_TimelineFirstAttack = 0;        // 1-based 首个攻击帧;0=未计算;-1=无攻击帧
};

auto GetHelper() -> CHelper*;
