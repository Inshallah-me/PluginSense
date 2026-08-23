#include "Hook_FireEventClientSide.hpp"

#include <PluginSenseClient/CPluginSenseClient.hpp>
#include <PluginSenseClient/Features/CBulletSparks/CBulletSparks.hpp>

auto Hook_FireEventClientSide( IGameEventManager2* pGameEventManager2 , IGameEvent* pGameEvent ) -> bool
{
	// bullet_impact 事件不走 FireEventClientSide,需在首次触发时注册监听器
	static bool s_registered = GetBulletSparks()->RegisterListener( pGameEventManager2 );

	GetPluginSenseClient()->OnFireEventClientSide( pGameEvent );

	return FireEventClientSide_o( pGameEventManager2 , pGameEvent );
}
