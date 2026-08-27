#include "CFakeCooldown.hpp"

#include <algorithm>
#include <ctime>

#include <Common/Common.hpp>
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
	// 冷却类型(写入 qword_1823DDA74 高 32 位),决定 GetCooldownType / GetCooldownReason / CooldownIsPermanent。
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

	// ------------------------------------------------------------------ client.dll 偏移(2026-08-27 更新,IDA 实测)
	// dword_1823DC9E0 : flag(0x200 = 有冷却)
	// qword_1823DCA74 : 低 32 位 = 冷却到期时间戳,高 32 位 = 冷却类型
	// dword_1823DCA7C : VAC 封禁标志(非零 = VAC 封禁)
	static constexpr uintptr_t OFF_COOLDOWN_FLAGS = 0x23DC9E0;
	static constexpr uintptr_t OFF_COOLDOWN_DATA = 0x23DCA74;
	static constexpr uintptr_t OFF_VAC_BAN = 0x23DCA7C;

	static uint64_t g_OriginalCooldownData = 0;
	static uint32_t g_OriginalFlags = 0;
	static uint32_t g_OriginalVacBan = 0;
	static bool g_OriginalSaved = false;

	// ------------------------------------------------------------------ 同步冷却 / VAC 封禁到内存
	// 冷却:写 qword_1823DDA74 + flag;Official Ban = 类型 8 永久;VAC 封禁:写 dword_1823DDA7C。
	// 三者互斥:有冷却时 VAC 不显示(游戏判定),VAC/Official Ban 开启时关冷却。
	void SyncAccountState()
	{
		auto base = reinterpret_cast<uintptr_t>( GetModuleHandleA( "client.dll" ) );
		if ( !base )
			return;

		const bool anyOn = menu_state::spoof && ( menu_state::fakeCooldown || menu_state::officialBan || menu_state::vacBan );

		if ( anyOn )
		{
			if ( !g_OriginalSaved )
			{
				g_OriginalCooldownData = *(uint64_t*)( base + OFF_COOLDOWN_DATA );
				g_OriginalFlags = *(uint32_t*)( base + OFF_COOLDOWN_FLAGS );
				g_OriginalVacBan = *(uint32_t*)( base + OFF_VAC_BAN );
				g_OriginalSaved = true;
			}

			if ( menu_state::vacBan )
			{
				// VAC 封禁:写标志,关冷却(类型 0,无到期时间)
				*(uint32_t*)( base + OFF_VAC_BAN ) = 1;
				*(uint32_t*)( base + OFF_COOLDOWN_FLAGS ) &= ~0x200u;
				*(uint64_t*)( base + OFF_COOLDOWN_DATA ) = 0;
				return;
			}

			// 冷却(含 Official Ban):VAC 不显示(置 0)
			*(uint32_t*)( base + OFF_VAC_BAN ) = 0;

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

			*(uint32_t*)( base + OFF_COOLDOWN_FLAGS ) |= 0x200;
			*(uint64_t*)( base + OFF_COOLDOWN_DATA ) = ( (uint64_t)(uint32_t)type << 32 ) | expiry;
		}
		else if ( g_OriginalSaved )
		{
			*(uint64_t*)( base + OFF_COOLDOWN_DATA ) = g_OriginalCooldownData;
			*(uint32_t*)( base + OFF_COOLDOWN_FLAGS ) = g_OriginalFlags;
			*(uint32_t*)( base + OFF_VAC_BAN ) = g_OriginalVacBan;
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
