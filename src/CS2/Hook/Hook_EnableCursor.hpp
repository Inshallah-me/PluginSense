#pragma once

#include <Common/Common.hpp>

auto Hook_EnableCursor( void* RCX , bool Active ) -> void*;
auto InstallHook_EnableCursor() -> bool;

using EnableCursor_t = decltype( &Hook_EnableCursor );
inline EnableCursor_t EnableCursor_o = nullptr;
inline bool EnableCursor_Input = true;
