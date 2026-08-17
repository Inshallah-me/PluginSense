#include "CChatSpammer.hpp"
#include <chrono>
#include <cstdio>
#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/Interface/IEngineToClient.hpp>
#include <PluginSenseClient/Features/CNameChanger/CNameChanger.hpp>

namespace menu_state
{
	extern bool chatSpammer;
	extern float sendDelay;
	extern int chatCount;
	extern char chatMessages[16][128];
}

static CChatSpammer g_CChatSpammer{};

void CChatSpammer::OnFrame()
{
	if ( !menu_state::chatSpammer || menu_state::chatCount == 0 )
		return;
	if ( !SDK::Interfaces::EngineToClient()->IsInGame() )
		return;

	static auto g_lastSend = std::chrono::steady_clock::now();
	static int g_msgIndex = 0;

	auto now = std::chrono::steady_clock::now();
	if ( std::chrono::duration<float>( now - g_lastSend ).count() < menu_state::sendDelay )
		return;
	g_lastSend = now;

	if ( g_msgIndex >= menu_state::chatCount )
		g_msgIndex = 0;

	if ( menu_state::chatMessages[ g_msgIndex ][ 0 ] ) {
		char cmd[ 288 ];
		snprintf( cmd, sizeof( cmd ), "say \"%s\"", menu_state::chatMessages[ g_msgIndex ] );
		GetNameChanger()->RunCommand( cmd );
	}

	g_msgIndex = ( g_msgIndex + 1 ) % menu_state::chatCount;
}

auto GetChatSpammer() -> CChatSpammer*
{
	return &g_CChatSpammer;
}
