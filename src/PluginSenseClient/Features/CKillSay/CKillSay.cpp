#include "CKillSay.hpp"
#include <cstdio>
#include <random>
#include <PluginSenseClient/Features/CNameChanger/CNameChanger.hpp>

namespace menu_state
{
	extern bool killSay;
	extern bool killSayRandom;
	extern int killCount;
	extern char killMessages[16][128];
}

static CKillSay g_CKillSay{};
static int g_msgIndex = 0;

void CKillSay::OnKill()
{
	if ( !menu_state::killSay || menu_state::killCount == 0 )
		return;

	// 随机模式:每次随机选一条;顺序模式:按索引循环
	int sendIndex = g_msgIndex;
	if ( menu_state::killSayRandom )
	{
		static std::mt19937 rng( std::random_device{}() );
		std::uniform_int_distribution<int> dist( 0, menu_state::killCount - 1 );
		sendIndex = dist( rng );
	}
	else if ( g_msgIndex >= menu_state::killCount )
	{
		g_msgIndex = 0;
		sendIndex = 0;
	}

	if ( menu_state::killMessages[ sendIndex ][ 0 ] ) {
		char cmd[ 288 ];
		snprintf( cmd, sizeof( cmd ), "say \"%s\"", menu_state::killMessages[ sendIndex ] );
		GetNameChanger()->RunCommand( cmd );
	}

	if ( !menu_state::killSayRandom )
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
