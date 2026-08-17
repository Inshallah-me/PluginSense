#include "Hook_MouseInputEnabled.hpp"

#include <PluginSenseClient/CPluginSenseGUI.hpp>

auto Hook_MouseInputEnabled( void* RCX ) -> bool
{
	if ( GetPluginSenseGUI()->IsVisible() )
		return false;

	return MouseInputEnabled_o( RCX );
}
