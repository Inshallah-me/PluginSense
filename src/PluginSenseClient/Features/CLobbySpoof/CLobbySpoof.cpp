#include "CLobbySpoof.hpp"

#include <MinHook/MinHook.h>

#include <Common/MemoryEngine.hpp>
#include <CS2/SDK/SDK.hpp>
#include <PluginSenseClient/Settings/MenuState.hpp>

// ============================================================================
// 大厅资料伪装(C++ 原生 hook)
//
// 数据源(已在 client.dll 当前版本验证,2026-09-01):
//   GetRankData(sub_180FF8710)       : 段位数值 + 胜场。out 低 32 位 = ranking,高 32 位 = wins。
//                                        mode: 11 = Premier, 7 = Wingman。
//   GetCurrentLevel(sub_180FEE8D0)   : 经验等级(GetFriendLevel/GetFriendXp 自己时都调用它)。
//   dword_1823DCA74 / dword_1823DCA78  : level / xppts 全局(由 flag 0x2000 / 0x4000 门控),
//                                        Game::SetPlayerRanking 广播时直接读取。
// ============================================================================

namespace
{
	// ------------------------------------------------------------------ 类型
	using GetRankDataFn = char( __fastcall* )( __int64 , __int64* , int );
	using GetCurrentLevelFn = int( __fastcall* )( );
	using RankByModeFn = __int64( __fastcall* )( __int64 , __int64 );
	using WinsByModeFn = __int64( __fastcall* )( __int64 , __int64 );

	static GetRankDataFn GetRankData_o = nullptr;
	static GetCurrentLevelFn GetCurrentLevel_o = nullptr;
	static RankByModeFn RankByMode_o = nullptr;
	static WinsByModeFn WinsByMode_o = nullptr;

	// ------------------------------------------------------------------ 特征码
	static const char* kGetRankDataPattern =
		"48 89 5C 24 10 44 89 44 24 18 55 57 41 57 48 8B EC 48 83 EC 60 48 8B FA";
	static const char* kGetCurrentLevelPattern =
		"48 83 EC 28 8B 05 ?? ?? ?? ?? C1 E8 0D A8 01 74 ?? 8B 05 ?? ?? ?? ?? 48 83 C4 28 C3";

	// PartyListAPI 自己 xuid 时读段位/胜场走这两个函数(按模式名:Competitive/Wingman/Premier)。
	// 两个函数开头字节完全相同,只能用"返回偏移"特征码区分(rank 返回 +0x54, wins 返回 +0x58)。
	static const char* kRankByModeReturnPattern =
		"48 69 C1 88 00 00 00 42 8B 44 18 54 48 83 C4 60 5D C3";
	static const char* kWinsByModeReturnPattern =
		"48 69 C1 88 00 00 00 42 8B 44 18 58 48 83 C4 60 5D C3";

	// ------------------------------------------------------------------ 全局偏移(client.dll RVA,2026-09-01 更新,IDA 实测)
	// dword_1823DC9D0 : profile 状态 flags(bit13=level 有效, bit14=xppts 有效)
	// dword_1823DCA74  : level 值
	// dword_1823DCA78  : xppts 值
	static constexpr uintptr_t OFF_PROFILE_FLAGS = 0x23DC9D0;
	static constexpr uintptr_t OFF_LEVEL_VALUE = 0x23DCA74;
	static constexpr uintptr_t OFF_XP_VALUE = 0x23DCA78;

	static constexpr uint32_t FLAG_LEVEL_VALID = 0x2000;
	static constexpr uint32_t FLAG_XP_VALID = 0x4000;

	// ------------------------------------------------------------------ 全局同步
	// SetPlayerRanking 广播时直接读这两个全局,写全局让房间其他人也能看到伪装值。
	// 放在 GetRankData hook 里调用(它被 UI 频繁调用,保证全局持续同步)。
	// 首次开启时保存原始值,关闭时还原,避免残留导致关不掉。
	static uint32_t g_originalProfileFlags = 0;
	static int g_originalLevel = 0;
	static int g_originalXp = 0;
	static bool g_originalProfileSaved = false;

	inline void SyncGlobalProfile()
	{
		auto base = reinterpret_cast<uintptr_t>( GetModuleHandleA( CLIENT_DLL ) );
		if ( !base )
			return;

		if ( menu_state::spoof && menu_state::lobbyLevel )
		{
			if ( !g_originalProfileSaved )
			{
				g_originalProfileFlags = *(uint32_t*)( base + OFF_PROFILE_FLAGS );
				g_originalLevel = *(int*)( base + OFF_LEVEL_VALUE );
				g_originalXp = *(int*)( base + OFF_XP_VALUE );
				g_originalProfileSaved = true;
			}

			*(uint32_t*)( base + OFF_PROFILE_FLAGS ) |= ( FLAG_LEVEL_VALID | FLAG_XP_VALID );
			*(int*)( base + OFF_LEVEL_VALUE ) = menu_state::lobbyLevelValue;
			*(int*)( base + OFF_XP_VALUE ) = menu_state::lobbyXp;
		}
		else if ( g_originalProfileSaved )
		{
			*(uint32_t*)( base + OFF_PROFILE_FLAGS ) = g_originalProfileFlags;
			*(int*)( base + OFF_LEVEL_VALUE ) = g_originalLevel;
			*(int*)( base + OFF_XP_VALUE ) = g_originalXp;
			g_originalProfileSaved = false;
		}
	}

	// ------------------------------------------------------------------ hooks
	char __fastcall hkGetRankData( __int64 a1 , __int64* out , int mode )
	{
		SyncGlobalProfile();

		const char result = GetRankData_o( a1 , out , mode );

		if ( menu_state::spoof )
		{
			// mode: 11 = Premier, 7 = Wingman。
			if ( mode == 11 && menu_state::lobbyPremier )
				out[0] = ( (uint64_t)(uint32_t)menu_state::lobbyPremierWins << 32 )
					| (uint32_t)menu_state::lobbyPremierRating;
			else if ( mode == 7 && menu_state::lobbyWingman )
				out[0] = ( (uint64_t)(uint32_t)menu_state::lobbyWingmanWins << 32 )
					| (uint32_t)menu_state::lobbyWingmanRating;
		}

		return result;
	}

	int __fastcall hkGetCurrentLevel()
	{
		if ( menu_state::spoof && menu_state::lobbyLevel )
			return menu_state::lobbyLevelValue;

		return GetCurrentLevel_o();
	}

	// 按模式名读段位数值(sub_180FF88D0)。PartyListAPI 自己 xuid 时调用,覆盖全部模式。
	__int64 __fastcall hkRankByMode( __int64 a1 , __int64 a2 )
	{
		if ( menu_state::spoof )
		{
			const char* szMode = reinterpret_cast<const char*>( a2 );
			if ( menu_state::lobbyPremier && szMode && _strcmpi( szMode , "Premier" ) == 0 )
				return menu_state::lobbyPremierRating;
			if ( menu_state::lobbyWingman && szMode && _strcmpi( szMode , "Wingman" ) == 0 )
				return menu_state::lobbyWingmanRating;
		}

		return RankByMode_o( a1 , a2 );
	}

	// 按模式名读胜场(sub_180FF94C0)。
	__int64 __fastcall hkWinsByMode( __int64 a1 , __int64 a2 )
	{
		if ( menu_state::spoof )
		{
			const char* szMode = reinterpret_cast<const char*>( a2 );
			if ( menu_state::lobbyPremier && szMode && _strcmpi( szMode , "Premier" ) == 0 )
				return menu_state::lobbyPremierWins;
			if ( menu_state::lobbyWingman && szMode && _strcmpi( szMode , "Wingman" ) == 0 )
				return menu_state::lobbyWingmanWins;
		}

		return WinsByMode_o( a1 , a2 );
	}

	// ------------------------------------------------------------------ 工具
	template<typename Fn>
	bool CreateHook( void* pTarget , Fn& oOriginal , void* pDetour )
	{
		if ( !pTarget )
			return false;

		MH_STATUS status = MH_CreateHook( pTarget , pDetour , reinterpret_cast<void**>( &oOriginal ) );
		if ( status != MH_OK )
			return false;

		return MH_EnableHook( pTarget ) == MH_OK;
	}

	// 特征码命中"函数内部某点",从这里往回扫描找函数入口。
	// 新版 prologue 可能带"保存寄存器"前缀(mov [rsp+18h], r14 等),找到 push rbp; mov rbp, rsp 后继续往前跳过。
	uint8_t* FindEntryFromInside( uint8_t* pInside )
	{
		for ( int i = 1; i < 0x400; ++i )
		{
			if ( pInside[-i] == 0x55 && pInside[-i + 1] == 0x48
				&& pInside[-i + 2] == 0x8B && pInside[-i + 3] == 0xEC )
			{
				uint8_t* pEntry = pInside - i;

				// 跳过函数入口前的"保存寄存器" prologue,如:
				//   mov [rsp+18h], r14  = 4C 89 74 24 18
				//   mov [rsp+10h], rbx  = 48 89 5C 24 10
				//   mov [rsp+10h], rsi  = 48 89 74 24 10
				//   mov [rsp+10h], rdi  = 48 89 7C 24 10
				while ( pEntry >= pInside - 0x400 )
				{
					if ( pEntry[-5] == 0x4C && pEntry[-4] == 0x89 &&
						 ( pEntry[-3] == 0x74 || pEntry[-3] == 0x7C ) && pEntry[-2] == 0x24 )
					{
						pEntry -= 5;
						continue;
					}

					if ( pEntry[-5] == 0x48 && pEntry[-4] == 0x89 &&
						 ( pEntry[-3] == 0x5C || pEntry[-3] == 0x74 || pEntry[-3] == 0x7C ) && pEntry[-2] == 0x24 )
					{
						pEntry -= 5;
						continue;
					}

					break;
				}

				return pEntry;
			}
		}

		return nullptr;
	}

	// 本模块安装的 hook 目标,Shutdown 时逐个禁用(不要用 MH_ALL_HOOKS 影响其他模块)。
	inline void* g_hookTargets[4] = {};
}

static CLobbySpoof g_CLobbySpoof{};

auto CLobbySpoof::Init() -> bool
{
	void* pRankData = FindPattern( CLIENT_DLL , kGetRankDataPattern );
	void* pCurrentLevel = FindPattern( CLIENT_DLL , kGetCurrentLevelPattern );
	void* pRankByMode = FindEntryFromInside(
		reinterpret_cast<uint8_t*>( FindPattern( CLIENT_DLL , kRankByModeReturnPattern ) ) );
	void* pWinsByMode = FindEntryFromInside(
		reinterpret_cast<uint8_t*>( FindPattern( CLIENT_DLL , kWinsByModeReturnPattern ) ) );

	// 先扫描定位,全部命中才装 hook(避免半装状态)。
	if ( !pRankData || !pCurrentLevel || !pRankByMode || !pWinsByMode )
		return false;

	g_hookTargets[0] = pRankData;
	g_hookTargets[1] = pCurrentLevel;
	g_hookTargets[2] = pRankByMode;
	g_hookTargets[3] = pWinsByMode;

	bool ok = true;
	ok &= CreateHook( pRankData , GetRankData_o , &hkGetRankData );
	ok &= CreateHook( pCurrentLevel , GetCurrentLevel_o , &hkGetCurrentLevel );
	ok &= CreateHook( pRankByMode , RankByMode_o , &hkRankByMode );
	ok &= CreateHook( pWinsByMode , WinsByMode_o , &hkWinsByMode );
	return ok;
}

auto CLobbySpoof::Shutdown() -> void
{
	for ( auto* pTarget : g_hookTargets )
	{
		if ( pTarget )
		{
			MH_DisableHook( pTarget );
			MH_RemoveHook( pTarget );
		}
	}

	for ( auto& pTarget : g_hookTargets )
		pTarget = nullptr;
}

auto GetLobbySpoof() -> CLobbySpoof*
{
	return &g_CLobbySpoof;
}
