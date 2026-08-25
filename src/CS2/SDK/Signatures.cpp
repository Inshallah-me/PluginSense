#include "Signatures.hpp"
#include <CS2/SDK/SDK.hpp>
#include <Common/MemoryEngine.hpp>

struct SigEntry
{
	const char* name;
	const char* dll;
	const char* pattern;
	uint32_t offset = 0;
	int type = 0; // eBasePatternSearchType
};

static SigEntry g_Sigs[] =
{
	{ XorStr( "PresentOverlay" ) , GAMEOVERLAYRENDER64_DLL , XorStr( "48 89 5C 24 ? 48 89 6C 24 ? 56 57 41 54 41 56 41 57 48 83 EC ? 41 8B F0 8B EA 4C 8B F9" ) } ,
	{ XorStr( "ResizeBuffers" ) , GAMEOVERLAYRENDER64_DLL , XorStr( "40 53 55 56 57 41 54 41 56 41 57 48 83 EC ? 44 8B E2" ) } ,
	{ XorStr( "CreateSwapChain" ) , GAMEOVERLAYRENDER64_DLL , XorStr( "40 53 55 56 57 48 83 EC ? 48 8B F9 49 8B F1 48 8D 0D ? ? ? ? 49 8B D8 48 8B EA E8 ? ? ? ? 48 8D 0D ? ? ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? E8 ? ? ? ? 48 8D 0D ? ? ? ? E8 ? ? ? ? 48 8B 05 ? ? ? ? 4C 8B CE 4C 8B C3 48 8B D5 48 8B CF FF D0 8B D8 85 C0 78 ? 48 85 F6 74 ? 48 83 3E ? 74 ? 48 8B D5 48 8B CE E8 ? ? ? ? 8B C3 48 83 C4 ? 5F 5E 5D 5B C3 CC CC CC CC CC CC CC CC CC CC CC CC CC CC CC 48 83 EC" ) } ,
	{ XorStr( "MouseInputEnabled" ) , CLIENT_DLL , XorStr( "40 53 48 83 EC 20 80 B9 ? ? ? ? ? 48 8B D9 75 78" ) } ,
	{ XorStr( "FireEventClientSide" ) , CLIENT_DLL , XorStr( "40 53 41 54 41 56 48 83 EC ? 4C 8B F2" ) } ,
	{ XorStr( "FrameStageNotify" ) , CLIENT_DLL , XorStr( "48 89 5C 24 ? 48 89 6C 24 ? 57 48 83 EC ? 48 8B F9 33 ED" ) } ,
	{ XorStr( "CreateMove" ) , CLIENT_DLL , XorStr( "85 D2 0F 85 ? ? ? ? 48 8B C4 44 88 40 18" ) } ,
	{ XorStr( "SerializePartialToArray" ) , CLIENT_DLL , XorStr( "48 89 5C 24 ? 55 56 57 48 81 EC ? ? ? ? 48 8B 05 ? ? ? ? 48 33 C4 48 89 84 24 ? ? ? ? 49 63 F0" ) } ,
	{ XorStr( "OnClientOutput" ) , ENGINE2_DLL , XorStr( "4C 8B DC 49 89 5B ? 49 89 6B ? 49 89 73 ? 57 41 56 41 57 48 83 EC ? 49 C7" ) } ,
	{ XorStr( "CDemoRecorder" ) , ENGINE2_DLL , XorStr( "40 56 57 41 57 48 83 EC ? 4C 8B F9" ) } ,
	{ XorStr( "IsRelativeMouseMode" ) , INPUTSYSTEM_DLL , XorStr( "48 89 6C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 41 56 48 83 EC ? 0F B6 F2" ) } ,
	{ XorStr( "AntiTamper" ) , CLIENT_DLL , XorStr( "40 53 41 57 48 83 EC ? 48 89 74 24 ? 48 8B F1" ) } ,
	{ XorStr( "IsLoadoutAllowed" ) , CLIENT_DLL , XorStr( "48 89 5C 24 ? 48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC ? 48 8B E9 48 8B 0D ? ? ? ? ? ? ? FF 50" ) } ,
	{ XorStr( "ProcessSubTickInput" ) , CLIENT_DLL , XorStr( "89 54 24 10 48 89 4C 24 08 53 56 57 48 83 EC 70" ) } ,
	{ XorStr( "ForceButtonsDown" ) , CLIENT_DLL , XorStr( "40 53 57 41 56 48 81 EC ? ? ? ? 48 83 79 ? 00" ) } ,
	{ XorStr( "FindHudElement" ) , CLIENT_DLL , XorStr( "40 53 48 83 EC ? 48 8B 05 ? ? ? ? 48 8B D9 48 85 C0 74 ? 48 89 5C 24" ) } ,
	{ XorStr( "PushNotice" ) , CLIENT_DLL , XorStr( "48 89 5C 24 ? 48 89 74 24 ? 48 89 7C 24 ? 55 41 56 41 57 48 8D AC 24 ? ? ? ? B8 ? ? ? ? E8 ? ? ? ? 48 2B E0 45 33 FF 41 8B D8" ) } ,
	{ XorStr( "OverrideView" ) , CLIENT_DLL , XorStr( "40 57 48 83 EC ? 48 8B FA E8 ? ? ? ? BA" ) } ,
	{ XorStr( "SetShaderParam" ) , CLIENT_DLL , XorStr( "E8 ? ? ? ? 0F B6 43 25" ) , 0 , SEARCH_TYPE_CALL } ,
	{ XorStr( "SetShaderParamI" ) , CLIENT_DLL , XorStr( "48 89 6C 24 ? 48 89 74 24 ? 57 48 83 EC 20 66 0F 6E CA 41 8B F0" ) } ,
	{ XorStr( "SetPostProcessVec" ) , ENGINE2_DLL , XorStr( "E8 ? ? ? ? 44 0F 28 94 24" ) , 0 , SEARCH_TYPE_CALL } ,
	{ XorStr( "AnalizePeModule" ) , CLIENT_DLL , XorStr( "48 8B C4 55 41 56 48 8D 68 ? 48 81 EC ? ? ? ? 48 89 58 ? 0F 57 C0" ) } ,
};

auto SIG( const char* name ) -> CBasePattern
{
	for ( auto& e : g_Sigs )
	{
		if ( _strcmpi( e.name, name ) == 0 )
			return CBasePattern( e.name, e.pattern, e.dll, e.offset, static_cast<eBasePatternSearchType>( e.type ) );
	}
	return CBasePattern( "", "", "" );
}

auto SIG_Fn( const char* name ) -> PVOID
{
	CBasePattern p = SIG( name );
	if ( !p.Search( true ) )
		return nullptr;
	return p.GetFunction();
}
