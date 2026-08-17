#pragma once
#include <Common/Common.hpp>

auto Hook_ForceButtonsDown( void* a1, __int64 a2 ) -> void;

using ForceButtonsDown_t = decltype( &Hook_ForceButtonsDown );
inline ForceButtonsDown_t ForceButtonsDown_o = nullptr;
