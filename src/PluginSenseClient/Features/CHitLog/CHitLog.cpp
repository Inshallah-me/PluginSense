#include "CHitLog.hpp"
#include <CS2/SDK/Signatures.hpp>
#include <CS2/SDK/SDK.hpp>

static CHitLog g_HitLog{};

using PushNoticeFn = __int64( __fastcall* )( void*, const char*, unsigned int, uint8_t* );
using FindHudElementFn = uintptr_t( __fastcall* )( const char* );

namespace
{
	PushNoticeFn g_fnPushNotice = nullptr;
	FindHudElementFn g_fnFindHud = nullptr;
	bool g_patternsLoaded = false;

	void EnsurePatterns()
	{
		if ( g_patternsLoaded ) return;
		g_fnFindHud = reinterpret_cast<FindHudElementFn>( SIG_Fn( XorStr( "FindHudElement" ) ) );
		g_fnPushNotice = reinterpret_cast<PushNoticeFn>( SIG_Fn( XorStr( "PushNotice" ) ) );
		g_patternsLoaded = true;
	}
}

void CHitLog::Send( const char* message )
{
	if ( !message || !message[0] )
		return;

	EnsurePatterns();

	if ( !g_fnFindHud || !g_fnPushNotice )
		return;

	const auto hud = g_fnFindHud( XorStr( "CCSGO_HudVoiceStatus" ) );
	if ( !hud || hud == 0x20 )
		return;

	uint8_t flags[2] = { 1, 0 };
	__try
	{
		g_fnPushNotice( reinterpret_cast<void*>( hud - 0x20 ), message, 0xFFFFFFFF, flags );
	}
	__except ( EXCEPTION_EXECUTE_HANDLER ) {}
}

auto GetHitLog() -> CHitLog*
{
	return &g_HitLog;
}
