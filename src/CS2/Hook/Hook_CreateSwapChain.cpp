#include "Hook_CreateSwapChain.hpp"

#include <PluginSenseClient/CPluginSenseGUI.hpp>

auto WINAPI Hook_CreateSwapChain( IDXGIFactory* pFactory , IUnknown* pDevice , DXGI_SWAP_CHAIN_DESC* pDesc , IDXGISwapChain** ppSwapChain )->HRESULT
{
	GetPluginSenseGUI()->ClearRenderTargetView();

	return CreateSwapChain_o( pFactory , pDevice , pDesc , ppSwapChain );
}
