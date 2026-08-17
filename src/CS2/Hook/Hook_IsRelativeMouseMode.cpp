#include "Hook_IsRelativeMouseMode.hpp"

#include <PluginSenseClient/CPluginSenseGUI.hpp>

auto Hook_IsRelativeMouseMode( CInputSystem* pInputSystem , bool Active ) -> void
{
	GetPluginSenseGUI()->m_bMainActive = Active;

	if ( GetPluginSenseGUI()->IsVisible() )
		return IsRelativeMouseMode_o( pInputSystem , false );

	return IsRelativeMouseMode_o( pInputSystem , Active );
}
