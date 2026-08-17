#include "Hook_OverrideView.hpp"

#include <PluginSenseClient/Features/CMotionCamera/CMotionCamera.hpp>

auto Hook_OverrideView( std::uintptr_t thisptr, std::uintptr_t view_setup ) -> void
{
	OverrideView_o( thisptr, view_setup );

	GetMotionCamera()->on_override_view( view_setup );
}
