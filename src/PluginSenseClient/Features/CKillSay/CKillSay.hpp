#pragma once

class CKillSay final
{
public:
	void OnKill();
	void ResetIndex();
};

auto GetKillSay() -> CKillSay*;
