#include "CVacNetReveal.hpp"

#include <PluginSenseClient/Settings/MenuState.hpp>

static CVacNetReveal g_CVacNetReveal{};

auto CVacNetReveal::Init() -> bool
{
	return VacNetReveal::Init();
}

auto CVacNetReveal::OnFrame() -> void
{
	VacNetReveal::OnFrame( menu_state::spoof && menu_state::vacnetEnabled );
}

auto GetVacNetReveal() -> CVacNetReveal*
{
	return &g_CVacNetReveal;
}
