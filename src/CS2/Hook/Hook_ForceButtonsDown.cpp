#include "Hook_ForceButtonsDown.hpp"
#include <PluginSenseClient/CPluginSenseGUI.hpp>

auto Hook_ForceButtonsDown( void* a1, __int64 a2 ) -> void
{
	if ( GetPluginSenseGUI()->IsVisible() )
		return;

	ForceButtonsDown_o( a1, a2 );
}
