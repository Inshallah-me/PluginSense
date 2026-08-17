#pragma once
#include <Common/Common.hpp>

auto Hook_ProcessSubTickInput( __int64 a1, int a2 ) -> __int64;

using ProcessSubTickInput_t = decltype( &Hook_ProcessSubTickInput );
inline ProcessSubTickInput_t ProcessSubTickInput_o = nullptr;
