#pragma once
#include <Common/Common.hpp>

class CHitLog final
{
public:
	void Send( const char* message );
};

auto GetHitLog() -> CHitLog*;
