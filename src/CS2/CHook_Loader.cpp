#include "CHook_Loader.hpp"

#include <Common/MemoryEngine.hpp>
#include <MinHook/MinHook.h>

#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/Signatures.hpp>

#include <CS2/Hook/Hook_Present.hpp>
#include <CS2/Hook/Hook_ResizeBuffers.hpp>
#include <CS2/Hook/Hook_CreateSwapChain.hpp>
#include <CS2/Hook/Hook_MouseInputEnabled.hpp>
#include <CS2/Hook/Hook_FireEventClientSide.hpp>
#include <CS2/Hook/Hook_FrameStageNotify.hpp>
#include <CS2/Hook/Hook_CreateMove.hpp>
#include <CS2/Hook/Hook_OnClientOutput.hpp>
#include <CS2/Hook/Hook_ParseMessage.hpp>
#include <CS2/Hook/Hook_IsRelativeMouseMode.hpp>
#include <CS2/Hook/Hook_EnableCursor.hpp>
#include <CS2/Hook/Hook_AntiTamper.hpp>
#include <CS2/Hook/Hook_IsLoadoutAllowed.hpp>
#include <CS2/Hook/Hook_ProcessSubTickInput.hpp>
#include <CS2/Hook/Hook_ForceButtonsDown.hpp>
#include <CS2/Hook/Hook_SetShaderParam.hpp>
#include <CS2/Hook/Hook_SetPostProcessVec.hpp>
#include <CS2/Hook/Hook_ServiceRead.hpp>
#include <CS2/Hook/Hook_OverrideView.hpp>


static CHook_Loader g_CHook_Loader{};

auto CHook_Loader::InitalizeMH() -> bool
{
	return MH_Initialize() == MH_OK;
}

auto CHook_Loader::InstallFirstHook() -> bool
{
	m_Hooks = {};

	return InstallHooks();
}

auto CHook_Loader::InstallSecondHook() -> bool
{
	m_Hooks =
	{
		{ SIG( XorStr( "PresentOverlay" ) ) , &Hook_Present , reinterpret_cast<LPVOID*>( &Present_o ) } ,
		{ SIG( XorStr( "ResizeBuffers" ) ) , &Hook_ResizeBuffers , reinterpret_cast<LPVOID*>( &ResizeBuffers_o ) } ,
		{ SIG( XorStr( "CreateSwapChain" ) ) , &Hook_CreateSwapChain , reinterpret_cast<LPVOID*>( &CreateSwapChain_o ) } ,
		{ SIG( XorStr( "MouseInputEnabled" ) ) , &Hook_MouseInputEnabled , reinterpret_cast<LPVOID*>( &MouseInputEnabled_o ) } ,
		{ SIG( XorStr( "FireEventClientSide" ) ) , &Hook_FireEventClientSide , reinterpret_cast<LPVOID*>( &FireEventClientSide_o ) } ,
		{ SIG( XorStr( "FrameStageNotify" ) ) , &Hook_FrameStageNotify , reinterpret_cast<LPVOID*>( &FrameStageNotify_o ) } ,
		{ SIG( XorStr( "CreateMove" ) ) , &Hook_CreateMove , reinterpret_cast<LPVOID*>( &CreateMove_o ) } ,
		{ SIG( XorStr( "SerializePartialToArray" ) ) , &Hook_MessageLite_SerializePartialToArray , reinterpret_cast<LPVOID*>( &MessageLite_SerializePartialToArray_o ) } ,
		{ SIG( XorStr( "OnClientOutput" ) ) , &Hook_OnClientOutput , reinterpret_cast<LPVOID*>( &OnClientOutput_o ) } ,
		{ SIG( XorStr( "CDemoRecorder" ) ) , &Hook_CDemoRecorder , reinterpret_cast<LPVOID*>( &CDemoRecorder_o ) } ,
		{ SIG( XorStr( "IsRelativeMouseMode" ) ) , &Hook_IsRelativeMouseMode , reinterpret_cast<LPVOID*>( &IsRelativeMouseMode_o ) } ,
		{ SIG( XorStr( "AntiTamper" ) ) , &Hook_AntiTamper , reinterpret_cast<LPVOID*>( &AntiTamper_o ) } ,
		{ SIG( XorStr( "IsLoadoutAllowed" ) ) , &Hook_IsLoadoutAllowed , reinterpret_cast<LPVOID*>( &IsLoadoutAllowed_o ) , true , true } ,
		{ SIG( XorStr( "ProcessSubTickInput" ) ) , &Hook_ProcessSubTickInput , reinterpret_cast<LPVOID*>( &ProcessSubTickInput_o ) } ,
	{ SIG( XorStr( "ForceButtonsDown" ) ) , &Hook_ForceButtonsDown , reinterpret_cast<LPVOID*>( &ForceButtonsDown_o ) } ,
		{ SIG( XorStr( "OverrideView" ) ) , &Hook_OverrideView , reinterpret_cast<LPVOID*>( &OverrideView_o ) , true , true } ,
		{ SIG( XorStr( "SetShaderParam" ) ) , &Hook_SetShaderParam , reinterpret_cast<LPVOID*>( &SetShaderParam_o ) , true , true } ,
		{ SIG( XorStr( "SetPostProcessVec" ) ) , &Hook_SetPostProcessVec , reinterpret_cast<LPVOID*>( &SetPostProcessVec_o ) , true , true } ,
	};

	if ( !InstallHooks() )
		return false;

	if ( !InstallHook_EnableCursor() )
			return false;

		{
			while ( !GetModuleHandleA( FILESYSTEM_STDIO_DLL ) )
				Sleep( 1 );

			const auto addr = reinterpret_cast<std::uintptr_t>(
				FindPattern( FILESYSTEM_STDIO_DLL, "00 48 89 07 48 8D 05 ? ? ? ? 48 89 87 E0 00 00 00" ) );
			if ( addr )
			{
				const auto ftable = GetPtrAddress<void**>( addr + 4 );
				if ( ftable )
				{
					const auto target = *ftable;
					if ( target )
					{
						MH_CreateHook( target, &Hook_ServiceRead, reinterpret_cast<LPVOID*>( &ServiceRead_o ) );
						MH_EnableHook( target );
					}
				}
			}
		}

		return true;
}

auto CHook_Loader::InstallHooks() -> bool
{
	for ( auto& Hook : m_Hooks )
	{
		while ( !GetModuleHandleA( Hook.m_Pattern.GetDllName() ) )
			   Sleep( 1 );

		if ( !Hook.m_Pattern.Search( Hook.m_bSkipError ) )
		{
			if ( !Hook.m_bSkipError )
				DEV_LOG( "[error] Hook #1 -> '%s'\n" , Hook.m_Pattern.GetPatternName() );

			if ( !Hook.m_bSkipIfNotFound )
				return false;
			else
				continue;
		}

		auto Status = MH_CreateHook( Hook.m_Pattern.GetFunction() , Hook.m_pDetour , Hook.m_pOriginal );

		if ( Status != MH_OK )
		{
			DEV_LOG( "[error] Hook #2 [%i] -> '%s'\n" , Status , Hook.m_Pattern.GetPatternName() );
			return false;
		}
	}

	MH_EnableHook( MH_ALL_HOOKS );

	m_Hooks.clear();

	return true;
}

auto CHook_Loader::DestroyHooks() -> void
{
	MH_DisableHook( MH_ALL_HOOKS );
	MH_RemoveHook( MH_ALL_HOOKS );

	MH_Uninitialize();
}

auto GetHook_Loader() -> CHook_Loader*
{
	return &g_CHook_Loader;
}
