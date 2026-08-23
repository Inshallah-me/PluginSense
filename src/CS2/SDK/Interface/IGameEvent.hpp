#pragma once

#include <Common/Common.hpp>

class CCSPlayerController;

struct EventKey
{
	uint32_t m_hash;
	uint32_t m_pad = 0xFFFFFFFF;
	const char* m_name;

	EventKey( const char* str );
};

class IGameEvent
{
public:
	auto GetName() -> const char*;
	auto GetInt64( const std::string_view Name ) -> int64_t;
	auto GetFloat( const std::string_view Name , float defaultValue = 0.f ) -> float;
	auto GetPlayerController( const std::string_view Name ) -> CCSPlayerController*;
	auto GetString( const std::string_view Name ) -> const char*;
	auto SetString( const std::string_view Name, const std::string_view Value ) -> const char*;
};
