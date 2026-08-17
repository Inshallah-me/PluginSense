#include "Hook_ProcessSubTickInput.hpp"
#include <PluginSenseClient/CPluginSenseGUI.hpp>

auto Hook_ProcessSubTickInput( __int64 a1, int a2 ) -> __int64
{
	auto Result = ProcessSubTickInput_o( a1, a2 );

	if ( GetPluginSenseGUI()->IsVisible() )
	{
		constexpr uint64_t kKeepKeys = ~( 1ULL | ( 1ULL << 11 ) );

		__int64 base = a1 + 2344LL * a2 + 552;

		*(uint64_t*)( base + 48 ) &= kKeepKeys;
		*(uint64_t*)( base + 56 ) = 0;
		*(uint64_t*)( base + 64 ) = 0;

		int count = *(int*)( base + 92 );
		uint8_t* entry = (uint8_t*)( base + 96 );
		for ( int i = 0; i < count && i < 32; ++i, entry += 32 )
		{
			*(uint64_t*)( entry + 0 ) &= kKeepKeys;
			*(uint64_t*)( entry + 8 ) = 0;
		}
	}

	return Result;
}
