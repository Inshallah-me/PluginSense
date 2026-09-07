#include "CFakeCooldown.hpp"

#include <algorithm>
#include <ctime>

#include <Common/Common.hpp>
#include <Common/DevLog.hpp>
#include <Common/MemoryEngine.hpp>
#include <DllLauncher.hpp>
#include <MinHook/MinHook.h>
#include <PluginSenseClient/Settings/MenuState.hpp>

namespace menu_state
{
	extern bool spoof;
	extern bool fakeCooldown;
	extern bool officialBan;
	extern bool vacBan;
	extern int fakeCooldownValue;
	extern int fakeCooldownTime;
	extern int fakeCooldownCustomDays;
}

namespace
{
	// ------------------------------------------------------------------ 冷却类型
	// 冷却类型(写入 qword_1823DCA64 高 32 位),决定 GetCooldownType / GetCooldownReason / CooldownIsPermanent。
	// 完整 1~23 与 GetCooldownReason 的 switch 对应,与 menu.cc 下拉框一一对应。
	// 顺序与 menu.cc 下拉框一一对应(作弊→行为→比赛→伤害→其他)
	static const int g_CooldownTypes[] =
	{
		11,  // Convicted Behavior
		10,  // Convicted Cheating
		19,  // GSLT Violation
		22,  // VacNet Culprit
		23,  // VacNet Affiliate
		21,  // Griefing
		5,   // Abandon
		12,  // Abandon Grace
		4,   // Disconnected
		13,  // Disconnect Grace
		16,  // Failed Connect
		1,   // Kicked
		9,   // Kicked Too Much
		17,  // Kick Abuse
		2,   // TK Limit
		3,   // TK Spawn
		6,   // TH Limit
		7,   // TH Spawn
		18,  // Skill Calibration
		15,  // Unknown(default)
	};
	static constexpr int kCooldownTypeCount = 20;

	// 冷却时长预设(秒):与 menu.cc 下拉框对应
	static const int g_CooldownTimes[] =
	{
		1800,      // 30 Mins
		72000,     // 20 Hours
		604800,    // 7 Days
		2592000,   // 30 Days
		15638400,  // 181 Days
		31536000,  // 365 Days
		315360000, // 3650 Days
		0,         // Custom(用 fakeCooldownCustomDays * 86400)
	};
	static constexpr int kCooldownTimeCount = 8;
	static constexpr int kCooldownTimeCustomIndex = 7;

	// ------------------------------------------------------------------ client.dll 冷却/VAC 全局:动态解析
	// 三个全局位于 .data(旧版实测 RVA 0x23DC9D0/0x23DCA64/0x23DCA6C,已废弃),
	// 改为从引用它们的指令里解 RIP 相对位移,随游戏更新自动重定位:
	//  · FLAGS : 已钩的 GetCooldownRemaining prologue 固定 `mov eax,[rip+d32]`
	//  · DATA  : 同函数内 `mov ebx,[rip+d32]` + timegm 调用 + `sub ebx,eax`(独立特征码)
	//  · VAC   : GetFriendIsVacBanned getter(22 字节整函数特征码)

	static uintptr_t g_AddrFlags = 0;
	static uintptr_t g_AddrData = 0;
	static uintptr_t g_AddrVac = 0;

	// 从 `8B /r disp32` 指令解 RIP 相对目标:位移存放处 + 后继地址 + disp
	static uintptr_t ResolveRipRef( uintptr_t dispAddr , uintptr_t nextInsn )
	{
		const int32_t disp = *reinterpret_cast<int32_t*>( dispAddr );
		return nextInsn + static_cast<uintptr_t>( disp );
	}

	static bool ResolveGlobals( uintptr_t hookTarget )
	{
		const auto base = reinterpret_cast<uintptr_t>( GetModuleHandleA( "client.dll" ) );
		if ( !base )
			return false;

		const auto sigData = XorStr( "8B 1D ? ? ? ? 48 8D 4C 24 ? FF 15 ? ? ? ? 2B D8" );
		const auto sigVac = XorStr( "8B 05 ? ? ? ? 85 C0 74 ? 83 3D ? ? ? ? 00 74 ? 33 C0 C3" );

		// FLAGS:特征码保证钩子函数 +4 处为 `8B 05 disp32`(小端 uint16 = 0x058B)
		if ( *reinterpret_cast<uint16_t*>( hookTarget + 4 ) != 0x058B )
			return false;
		g_AddrFlags = ResolveRipRef( hookTarget + 6 , hookTarget + 10 );

		if ( const auto ref = reinterpret_cast<uintptr_t>( FindPattern( XorStr( "client.dll" ) , sigData ) ) )
			g_AddrData = ResolveRipRef( ref + 2 , ref + 6 );

		if ( const auto ref = reinterpret_cast<uintptr_t>( FindPattern( XorStr( "client.dll" ) , sigVac ) ) )
			g_AddrVac = ResolveRipRef( ref + 2 , ref + 6 );

		// 边界校验:三个地址必须落在 client.dll 模块范围内(37MB,取 64MB 上界)
		const uintptr_t lo = base + 0x1000;
		const uintptr_t hi = base + 0x4000000;
		const bool ok = g_AddrFlags > lo && g_AddrFlags < hi
			&& g_AddrData > lo && g_AddrData < hi
			&& g_AddrVac > lo && g_AddrVac < hi;

		if ( !ok )
		{
			g_AddrFlags = g_AddrData = g_AddrVac = 0;
			DEV_LOG( "[cooldown] global resolve failed - feature disabled" );
		}
		return ok;
	}

	static uint64_t g_OriginalCooldownData = 0;
	static uint32_t g_OriginalFlags = 0;
	static uint32_t g_OriginalVacBan = 0;
	static bool g_OriginalSaved = false;

	// ------------------------------------------------------------------ 同步冷却 / VAC 封禁到内存
	// 冷却:写 qword_1823DCA64 + flag;Official Ban = 类型 8 永久;VAC 封禁:写 dword_1823DCA6C。
	// 三者互斥:有冷却时 VAC 不显示(游戏判定),VAC/Official Ban 开启时关冷却。
	void SyncAccountState()
	{
		// 全局未解析成功时整体禁用,不做任何内存写入
		if ( !g_AddrFlags || !g_AddrData || !g_AddrVac )
			return;

		const bool anyOn = menu_state::spoof && ( menu_state::fakeCooldown || menu_state::officialBan || menu_state::vacBan );

		if ( anyOn )
		{
			if ( !g_OriginalSaved )
			{
				g_OriginalCooldownData = *(uint64_t*)( g_AddrData );
				g_OriginalFlags = *(uint32_t*)( g_AddrFlags );
				g_OriginalVacBan = *(uint32_t*)( g_AddrVac );
				g_OriginalSaved = true;
			}

			if ( menu_state::vacBan )
			{
				// VAC 封禁:写标志,关冷却(类型 0,无到期时间)
				*(uint32_t*)( g_AddrVac ) = 1;
				*(uint32_t*)( g_AddrFlags ) &= ~0x200u;
				*(uint64_t*)( g_AddrData ) = 0;
				return;
			}

			// 冷却(含 Official Ban):VAC 不显示(置 0)
			*(uint32_t*)( g_AddrVac ) = 0;

			int type = 0;
			int seconds = 0;

			if ( menu_state::officialBan )
			{
				type = 8;              // Official Ban
				seconds = 315360000;   // 永久 ~10 年
			}
			else
			{
				const int idx = (std::clamp)( menu_state::fakeCooldownValue , 0 , kCooldownTypeCount - 1 );
				type = g_CooldownTypes[idx];
				const int timeIdx = (std::clamp)( menu_state::fakeCooldownTime , 0 , kCooldownTimeCount - 1 );
				seconds = g_CooldownTimes[timeIdx];
				if ( timeIdx == kCooldownTimeCustomIndex )
				{
					seconds = menu_state::fakeCooldownCustomDays * 86400;
					if ( seconds < 1 )
						seconds = 86400;
				}
			}

			const auto now = time( nullptr );
			const uint32_t expiry = static_cast<uint32_t>( now ) + static_cast<uint32_t>( seconds );

			*(uint32_t*)( g_AddrFlags ) |= 0x200;
			*(uint64_t*)( g_AddrData ) = ( (uint64_t)(uint32_t)type << 32 ) | expiry;
		}
		else if ( g_OriginalSaved )
		{
			*(uint64_t*)( g_AddrData ) = g_OriginalCooldownData;
			*(uint32_t*)( g_AddrFlags ) = g_OriginalFlags;
			*(uint32_t*)( g_AddrVac ) = g_OriginalVacBan;
			g_OriginalSaved = false;
		}
	}

	// ------------------------------------------------------------------ hooks
	using GetCooldownFn = int( __fastcall* )();
	static GetCooldownFn g_original = nullptr;

	int __fastcall hkGetCooldownRemaining()
	{
		// 同步冷却 / Official Ban / VAC 状态到内存(GetCooldownType / GetCooldownReason / GetFriendIsVacBanned 读同一数据)。
		SyncAccountState();

		if ( menu_state::spoof && ( menu_state::fakeCooldown || menu_state::officialBan || menu_state::vacBan ) )
		{
			if ( menu_state::vacBan )
				return 0; // VAC 封禁无冷却时间

			if ( menu_state::officialBan )
				return 315360000; // Official Ban 永久 ~10 年

			// 冷却:直接返回秒数(与游戏时间基准无关,避免时区偏移导致剩余时间算错)。
			const int timeIdx = (std::clamp)( menu_state::fakeCooldownTime , 0 , kCooldownTimeCount - 1 );
			int seconds = g_CooldownTimes[timeIdx];
			if ( timeIdx == kCooldownTimeCustomIndex )
			{
				seconds = menu_state::fakeCooldownCustomDays * 86400;
				if ( seconds < 1 )
					seconds = 86400;
			}
			return seconds;
		}

		return g_original();
	}
}

static CFakeCooldown g_CFakeCooldown{};
static void* g_pTarget = nullptr;

auto CFakeCooldown::Init() -> bool
{
	const auto pattern = XorStr( "48 83 EC ? 8B 05 ? ? ? ? C1 E8 ? A8 ? 74 ? 48 8D 4C 24" );

	g_pTarget = FindPattern( XorStr( "client.dll" ), pattern );
	if ( !g_pTarget )
		return false;

	// 冷却/VAC 全局随钩子目标动态解析;失败即整个功能不启用
	if ( !ResolveGlobals( reinterpret_cast<uintptr_t>( g_pTarget ) ) )
		return false;

	g_original = reinterpret_cast<GetCooldownFn>( g_pTarget );

	MH_STATUS status = MH_CreateHook( g_pTarget , hkGetCooldownRemaining , reinterpret_cast<void**>( &g_original ) );
	if ( status != MH_OK )
		return false;

	return MH_EnableHook( g_pTarget ) == MH_OK;
}

auto CFakeCooldown::Shutdown() -> void
{
	// 恢复冷却内存
	SyncAccountState();

	if ( g_pTarget )
	{
		MH_DisableHook( g_pTarget );
		MH_RemoveHook( g_pTarget );
		g_pTarget = nullptr;
	}
}

auto GetFakeCooldown() -> CFakeCooldown*
{
	return &g_CFakeCooldown;
}
