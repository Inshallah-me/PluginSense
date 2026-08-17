#pragma once

struct ImDrawList;
class IGameEvent;

class CFortniteDamage final
{
public:
	void Init();
	void OnPlayerHurt( IGameEvent* event );
	void OnRender( ImDrawList* drawList );
	void OnLevelShutdown();
};

auto GetFortniteDamage() -> CFortniteDamage*;
