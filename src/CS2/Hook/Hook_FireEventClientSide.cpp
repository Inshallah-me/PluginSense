#include "Hook_FireEventClientSide.hpp"

#include <PluginSenseClient/CPluginSenseClient.hpp>

auto Hook_FireEventClientSide( IGameEventManager2* pGameEventManager2 , IGameEvent* pGameEvent ) -> bool
{
	GetPluginSenseClient()->OnFireEventClientSide( pGameEvent );

	return FireEventClientSide_o( pGameEventManager2 , pGameEvent );
}
