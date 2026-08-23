#include "IGameEvent.hpp"

#include <CS2/SDK/FunctionListSDK.hpp>
#include <CS2/SDK/Types/CUtlStringToken.hpp>

static uint32_t MurmurHash2( const char* str, int len, uint32_t seed )
{
	const uint32_t m = 0x5bd1e995;
	const int r = 24;
	uint32_t h = seed ^ len;
	const unsigned char* data = (const unsigned char*)str;

	while ( len >= 4 )
	{
		uint32_t k = *(uint32_t*)data;
		k *= m; k ^= k >> r; k *= m;
		h *= m; h ^= k;
		data += 4; len -= 4;
	}
	switch ( len )
	{
	case 3: h ^= data[ 2 ] << 16;
	case 2: h ^= data[ 1 ] << 8;
	case 1: h ^= data[ 0 ]; h *= m;
	}
	h ^= h >> 13;
	h *= m;
	h ^= h >> 15;
	return h;
}

EventKey::EventKey( const char* str )
	: m_name( str ), m_hash( MurmurHash2( str, (int)strlen( str ), 0x31415926 ) ) {}

auto IGameEvent::GetName() -> const char*
{
	return IGameEvent_GetName( this );
}

auto IGameEvent::GetInt64( const std::string_view Name ) -> int64_t
{
	EventKey key( Name.data() );
	using Fn = int( __fastcall* )( IGameEvent*, EventKey, int );
	return ( *reinterpret_cast<Fn**>( this ) )[ 7 ]( this, key, 0 );
}

auto IGameEvent::GetFloat( const std::string_view Name , float defaultValue ) -> float
{
	return IGameEvent_GetFloat( this , Name.data() , defaultValue );
}

auto IGameEvent::GetPlayerController( const std::string_view Name ) -> CCSPlayerController*
{
	CUtlStringToken Token( Name.data() );

	return IGameEvent_GetPlayerController( this , &Token );
}

auto IGameEvent::GetString( const std::string_view Name ) -> const char*
{
	CUtlStringToken Token( Name.data() );

	return IGameEvent_GetString( this , &Token );
}

auto IGameEvent::SetString( const std::string_view Name , const std::string_view Value ) -> const char*
{
	CUtlStringToken Token( Name.data() );

	return IGameEvent_SetString( this , &Token , Value.data() );
}
