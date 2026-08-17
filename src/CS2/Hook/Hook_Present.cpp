#include "Hook_Present.hpp"

#include <PluginSenseClient/CPluginSenseGUI.hpp>

auto Hook_Present( IDXGISwapChain* pSwapChain , UINT SyncInterval , UINT Flags ) -> HRESULT
{
	GetPluginSenseGUI()->OnPresent( pSwapChain );

	return Present_o( pSwapChain , SyncInterval , Flags );
}
