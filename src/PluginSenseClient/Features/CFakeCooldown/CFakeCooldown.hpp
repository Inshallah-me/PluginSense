#pragma once

class CFakeCooldown final
{
public:
	auto Init() -> bool;
	auto Shutdown() -> void;
};

auto GetFakeCooldown() -> CFakeCooldown*;
