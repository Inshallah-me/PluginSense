#include "CKillSay.hpp"
#include <cstdio>
#include <PluginSenseClient/Features/CNameChanger/CNameChanger.hpp>

namespace menu_state
{
	extern bool killSay;
	extern int killCount;
	extern char killMessages[16][128];
}

static CKillSay g_CKillSay{};
static int g_msgIndex = 0;

void CKillSay::OnKill()
{
	if ( !menu_state::killSay || menu_state::killCount == 0 )
		return;

	if ( g_msgIndex >= menu_state::killCount )
		g_msgIndex = 0;

	if ( menu_state::killMessages[ g_msgIndex ][ 0 ] ) {
		char cmd[ 288 ];
		snprintf( cmd, sizeof( cmd ), "say \"%s\"", menu_state::killMessages[ g_msgIndex ] );
		GetNameChanger()->RunCommand( cmd );
	}

	g_msgIndex = ( g_msgIndex + 1 ) % menu_state::killCount;
}

void CKillSay::ResetIndex()
{
	g_msgIndex = 0;
}

auto GetKillSay() -> CKillSay*
{
	return &g_CKillSay;
}
