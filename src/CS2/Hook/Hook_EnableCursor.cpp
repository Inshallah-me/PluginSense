#include "Hook_EnableCursor.hpp"

#include <MinHook/MinHook.h>

#include <PluginSenseClient/CPluginSenseGUI.hpp>
#include <CS2/SDK/SDK.hpp>

auto Hook_EnableCursor( void* RCX , bool Active ) -> void*
{
	if ( !EnableCursor_o )
		return nullptr;

	EnableCursor_Input = Active;

	if ( GetPluginSenseGUI()->IsVisible() )
		Active = false;

	return EnableCursor_o( RCX , Active );
}

auto InstallHook_EnableCursor() -> bool
{
	auto pInputSystem = SDK::Interfaces::InputSystem();
	if ( !pInputSystem )
		return false;

	auto pVTable = *reinterpret_cast<void***>( pInputSystem );
	if ( !pVTable || !pVTable[ 76 ] )
		return false;

	auto Status = MH_CreateHook( pVTable[ 76 ] , &Hook_EnableCursor , reinterpret_cast<LPVOID*>( &EnableCursor_o ) );
	if ( Status != MH_OK && Status != MH_ERROR_ALREADY_CREATED )
	{
		DEV_LOG( "[error] Hook::EnableCursor [%i]\n" , Status );
		return false;
	}

	Status = MH_EnableHook( pVTable[ 76 ] );
	if ( Status != MH_OK && Status != MH_ERROR_ENABLED )
	{
		DEV_LOG( "[error] Hook::EnableCursor enable [%i]\n" , Status );
		return false;
	}

	return true;
}
