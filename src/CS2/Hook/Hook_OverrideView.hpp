#pragma once

#include <cstdint>

auto Hook_OverrideView( std::uintptr_t thisptr, std::uintptr_t view_setup ) -> void;

using OverrideView_t = decltype( &Hook_OverrideView );
inline OverrideView_t OverrideView_o = nullptr;
