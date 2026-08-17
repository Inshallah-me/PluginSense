#pragma once

#include <Common/Common.hpp>
#include <d3d11.h>

class CPluginSenseMenu final
{
public:
	auto OnGuiInitialized( IDXGISwapChain* pSwapChain , ID3D11Device* pDevice , ID3D11DeviceContext* pDeviceContext ) -> void;
	auto OnRenderMenu() -> void;
	auto OnRenderWidgets() -> void;
};

auto GetPluginSenseMenu() -> CPluginSenseMenu*;
