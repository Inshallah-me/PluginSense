#include "CPluginSenseMenu.hpp"

#include <PluginSenseClient/Assets/fortnite_digits.hpp>
#include <PluginSenseClient/Assets/FredokaOne_Regular.hpp>
#include <framework_w/includes.hh>

namespace vel { inline ImFont* g_font = nullptr; }

static CPluginSenseMenu g_CPluginSenseMenu{};

namespace
{
	void UpdateOverlaySizeFromSwapChain()
	{
		int width = 0;
		int height = 0;

		if (core::g_overlay->m_swap_chain)
		{
			DXGI_SWAP_CHAIN_DESC desc{};
			if (SUCCEEDED(core::g_overlay->m_swap_chain->GetDesc(&desc)))
			{
				width = static_cast<int>(desc.BufferDesc.Width);
				height = static_cast<int>(desc.BufferDesc.Height);

				if ((width <= 0 || height <= 0) && desc.OutputWindow)
				{
					RECT rect{};
					if (GetClientRect(desc.OutputWindow, &rect))
					{
						width = rect.right - rect.left;
						height = rect.bottom - rect.top;
					}
				}
			}
		}

		if (width <= 0 || height <= 0)
		{
			width = static_cast<int>(ImGui::GetIO().DisplaySize.x);
			height = static_cast<int>(ImGui::GetIO().DisplaySize.y);
		}

		if (width > 0 && height > 0)
		{
			core::g_overlay->width = width;
			core::g_overlay->height = height;
		}
	}
}

auto CPluginSenseMenu::OnGuiInitialized( IDXGISwapChain* pSwapChain , ID3D11Device* pDevice , ID3D11DeviceContext* pDeviceContext ) -> void
{
	DXGI_SWAP_CHAIN_DESC desc{};
	pSwapChain->GetDesc( &desc );

	core::g_overlay->width = desc.BufferDesc.Width;
	core::g_overlay->height = desc.BufferDesc.Height;
	core::g_overlay->m_device = pDevice;
	core::g_overlay->m_context = pDeviceContext;
	core::g_overlay->m_swap_chain = pSwapChain;

	g_render->setup();
	framework::g_menu->initialize();
	fortnite_digits::Init( pDevice );

	ImFontConfig velCfg;
	velCfg.FontDataOwnedByAtlas = false;
	velCfg.OversampleH = 2;
	velCfg.OversampleV = 2;
	velCfg.PixelSnapH = true;
	vel::g_font = ImGui::GetIO().Fonts->AddFontFromMemoryTTF(
		const_cast<unsigned char*>( font_data::FredokaOne_Regular ),
		static_cast<int>( font_data::FredokaOne_Regular_size ),
		32.f, &velCfg, ImGui::GetIO().Fonts->GetGlyphRangesDefault() );
}

auto CPluginSenseMenu::OnRenderMenu() -> void
{
	UpdateOverlaySizeFromSwapChain();

	g_render->set_draw_list( ImGui::GetBackgroundDrawList() );
	g_render->begin_layers();

	framework::g_ctx->m_open = true;
	framework::g_menu->runtime();

	g_render->end_layers();
}

auto CPluginSenseMenu::OnRenderWidgets() -> void
{
	UpdateOverlaySizeFromSwapChain();

	g_render->set_draw_list( ImGui::GetBackgroundDrawList() );
	g_render->begin_layers();

	framework::g_ctx->m_open = false;
	framework::g_menu->render_widgets();

	g_render->end_layers();
}

auto GetPluginSenseMenu() -> CPluginSenseMenu*
{
	return &g_CPluginSenseMenu;
}
