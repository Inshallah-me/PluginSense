#pragma once

#include <Common/Common.hpp>

class IGameEvent;
class C_CSPlayerPawn;

class CDamageEffect final
{
public:
	auto OnInit() -> void;
	auto OnPlayerHurt( IGameEvent* pGameEvent ) -> void;

private:
	void PlayDeathEffect( C_CSPlayerPawn* pPawn );

private:
	bool m_bLoaded = false;
};

auto GetDamageEffect() -> CDamageEffect*;
