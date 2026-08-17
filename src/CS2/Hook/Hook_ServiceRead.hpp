#pragma once

#include <cstdint>

auto Hook_ServiceRead( std::uintptr_t a1 ) -> std::uintptr_t;

using ServiceRead_t = decltype( &Hook_ServiceRead );
inline ServiceRead_t ServiceRead_o = nullptr;
