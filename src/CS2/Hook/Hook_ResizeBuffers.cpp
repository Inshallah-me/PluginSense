#include "Hook_ResizeBuffers.hpp"

#include <PluginSenseClient/CPluginSenseGUI.hpp>

auto Hook_ResizeBuffers( IDXGISwapChain* pSwapChain , UINT BufferCount , UINT Width , UINT Height , DXGI_FORMAT NewFormat , UINT SwapChainFlags ) -> HRESULT
{
	GetPluginSenseGUI()->OnResizeBuffers( pSwapChain );

	const auto result = ResizeBuffers_o( pSwapChain , BufferCount , Width , Height , NewFormat , SwapChainFlags );

	if ( SUCCEEDED( result ) )
		GetPluginSenseGUI()->OnResizeBuffersPost( pSwapChain );

	return result;
}
