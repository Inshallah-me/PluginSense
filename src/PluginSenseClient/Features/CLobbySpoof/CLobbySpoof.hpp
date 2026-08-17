#pragma once

#include <Common/Common.hpp>

class CLobbySpoof final
{
public:
	auto Init() -> bool;
	auto Shutdown() -> void;
};

auto GetLobbySpoof() -> CLobbySpoof*;
