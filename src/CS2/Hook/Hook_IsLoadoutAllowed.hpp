#pragma once

#include <common/Common.hpp>

auto Hook_IsLoadoutAllowed() -> bool;

using IsLoadoutAllowed_t = decltype(&Hook_IsLoadoutAllowed);
inline IsLoadoutAllowed_t IsLoadoutAllowed_o = nullptr;
