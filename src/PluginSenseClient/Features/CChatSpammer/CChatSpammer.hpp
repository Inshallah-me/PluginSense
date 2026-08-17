#pragma once

class CChatSpammer final
{
public:
	void OnFrame();
};

auto GetChatSpammer() -> CChatSpammer*;
