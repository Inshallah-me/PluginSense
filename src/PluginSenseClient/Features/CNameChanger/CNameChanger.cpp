#include "CNameChanger.hpp"
#include <Common/Common.hpp>
#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/Interface/IEngineCvar.hpp>
#include <CS2/SDK/Interface/IEngineToClient.hpp>
#include <CS2/SDK/Types/CEntityData.hpp>
#include <GameClient/CL_Players.hpp>
#include <chrono>
#include <random>
#include <algorithm>
#include <cstdio>
#include <cstring>

// ============================================================================
// RunCommand — execute any console command via InputService
// ============================================================================
auto CNameChanger::RunCommand( const char* cmd ) -> void
{
	static void* s_input = nullptr;
	static void( __fastcall** s_exec )( void*, int, const char*, int ) = nullptr;
	if ( !s_input ) {
		HMODULE eng = GetModuleHandleW( L"engine2.dll" );
		if ( !eng ) return;
		auto create = (void*( __cdecl* )( const char*, int* ))GetProcAddress( eng, "CreateInterface" );
		s_input = create ? create( "InputService_001", nullptr ) : nullptr;
		if ( s_input ) {
			void** vt = *(void***)s_input;
			s_exec = (void( __fastcall** )( void*, int, const char*, int ))&vt[ 25 ];
		}
	}
	if ( s_input && s_exec ) ( *s_exec )( s_input, 5, cmd, 0 );
}

static void RunNameCommand( const char* name )
{
	if ( !name || !name[ 0 ] ) return;
	char cmd[ 512 ];
	snprintf( cmd, sizeof( cmd ), "name \"%s\"; setinfo name \"%s\"", name, name );
	GetNameChanger()->RunCommand( cmd );
}

// ============================================================================
// Config sync — read from menu_state (defined in tabs.cpp)
// ============================================================================
namespace menu_state
{
	extern bool clantagEnabled;
	extern int clantagSelection;
	extern int animationSelection;
	extern float clantagSpeed;
	extern char playerName[ 32 ];
	extern char customClantag[ 32 ];
	extern bool animatedClantag;
}

// ============================================================================
// Preset animation data (22 presets)
// ============================================================================
struct Preset { const char* name; const char* const* frames; int count; };

static const char* _Fatality_win[] = {
	"f","fa","fat","fata","fatal","fatali","fatalit","fatality","fatality.","fatality.w",
	"fatality.wi","fatality.win","fatality.win","fatality.win","fatality.win","fatality.win",
	"fatality.win","fatality.win","fatality.win","fatality.win","fatality.win","fatality.win",
	"fatality.wi","fatality.w","fatality.","fatality","fatalit","fatali","fatal","fata","fat","fa","f"
};
static const char* _Aimware_net[] = {
	"AIMWARE.NET ","IMWARE.NET A","MWARE.NET AI","WARE.NET AIM","ARE.NET AIMW",
	"RE.NET AIMWA","E.NET AIMWAR",".NET AIMWARE","NET AIMWARE.","ET AIMWARE.N",
	"T AIMWARE.NE"," AIMWARE.NET","AIMWARE.NET ","AIMWARE.NET  "
};
static const char* _Iniuria_us[] = {
	"I\t\t ","IN\t\t","INI\t   ","INIU\t  ","INIUR\t ","INIURI\t","INIURIA   ",
	"INIURIA.  ","INIURIA.U ","INIURIA.US","INIURIA.US"," NIURIA.US","  IURIA.US",
	"   URIA.US","\tRIA.US","\t IA.US","\t  A.US","\t   .US","\t\tUS","\t\t S"
};
static const char* _Gamesense_pub[] = {
	"                  ","                 g","                ga","               gam",
	"              game","             games","            gamese","           gamesen",
	"          gamesens","         gamesense","        gamesense ","       gamesense  ",
	"      gamesense   ","     gamesense    ","    gamesense     ","   gamesense      ",
	"  gamesense       "," gamesense        ","gamesense         ","amesense          ",
	"mesense           ","esense            ","sense             ","sens              ",
	"sen               ","se                ","s                 "
};
static const char* _Nixware_cc[] = {
	"\t\t\t  n","\t\t\t ni","\t\t\tnix","\t\t   nixw","\t\t  nixwa",
	"\t\t nixwar","\t\tnixware","\t   nixware.","\t  nixware.c","\t nixware.cc",
	"\tnixware.cc","   nixware.c ","  nixware.  "," nixware   ","nixwar\t",
	"nixwa\t ","nixw\t  ","nix\t   ","ni\t\t","n\t\t "
};
static const char* _Neverlose_cc[] = {
	"N","N3","Ne","Ne\\","Ne\\/","Nev","Nev3","Neve","Neve|","Neve|2",
	"Never","Never|","Never|_","Neverl","Neverl0","Neverlo","Neverlo5",
	"Neverlos","Neverlos3","Neverlose","Neverlose.","Neverlose.<","Neverlose.c",
	"Neverlose.c<","Neverlose.cc","Neverlose.cc ","Neverlose.cc ","Neverlose.cc",
	"Neverlose.c<","Neverlose.c","Neverlose.<","Neverlose.","Neverlose",
	"Neverlos3","Neverlos","Neverlo5","Neverlo","Neverl0","Neverl","Never|_",
	"Never|","Never","Neve|2","Neve|","Neve","Nev3","Nev","Ne\\/","Ne\\","Ne","N3","N"
};
static const char* _Primordial_dev[] = {
	"\xe2\x8c\x9b ","\xe2\x8c\x9b p","\xe2\x8c\x9b pr","\xe2\x8c\x9b pri",
	"\xe2\x8c\x9b prim","\xe2\x8c\x9b primo","\xe2\x8c\x9b primor","\xe2\x8c\x9b primord",
	"\xe2\x8c\x9b primordi","\xe2\x8c\x9b primordia","\xe2\x8c\x9b primordial",
	"\xe2\x8c\x9b primordial","\xe2\x8c\x9b primordia","\xe2\x8c\x9b primordi",
	"\xe2\x8c\x9b primord","\xe2\x8c\x9b primor","\xe2\x8c\x9b primo",
	"\xe2\x8c\x9b prim","\xe2\x8c\x9b pri","\xe2\x8c\x9b pr","\xe2\x8c\x9b p","\xe2\x8c\x9b "
};
static const char* _Skeet_cc[] = {
	" s"," sk"," ske"," skee"," skee."," skeet."," skeet.c"," skeet.cc",
	" skeet.cc "," skeet.cc "," skeet.cc "," skeet.cc "," skeet.cc ",
	" skeet.cc "," skeet.cc "," skeet.cc "," skeet.cc "," skeet.cc ",
	" skeet.cc "," skeet.cc "," skeet.c "," skeet. "," skeet ",
	" skee "," ske "," sk "," s ","  "," "
};
static const char* _Millionware[] = {
	" millionware "," e millionwar"," re millionwa"," are millionw",
	" ware million"," nware millio"," onware milli"," ionware mill",
	" lionware mil"," llionware mi"," illionware m"," millionware"
};
static const char* _Legendware[] = {
	"l ","le ","leg ","lege ","legen ","legend ","legendw ",
	"legendwa ","legendwar ","legendware ","legendware ",
	"legendwar ","legendwa ","legendw ","legend ","legen ",
	"lege ","leg ","le ","l "
};
static const char* _Rifk[] = {
	" [] "," [ R] "," [ Ri] "," [ Rif] "," [ Rifk '] ",
	" [ Rifk '] "," [ Rifk '] "," [ Rifk] "," [ Rif] ",
	" [ Ri] "," [ R] "," [] "
};
static const char* _Weave[] = {
	"WEAVE.SU","W3AVE.SU","W34VE.SU","WE4V3.SU","WEAV3.5U",
	"W3AV3.5U","W34V3.5U","&E4VE.SU","$E@%^.S+","$!@%^.?;"
};
static const char* _SpirtHack[] = {
	"\xe2\x97\x87 ","\xe2\x97\x87 ","\xe2\x97\x87 S ","\xe2\x97\x87 Sp ","\xe2\x97\x87 Spi ","\xe2\x97\x87 Spir ","\xe2\x97\x87 Spirt ",
	"\xe2\x97\x87 SpirtH ","\xe2\x97\x87 SpirtHa ","\xe2\x97\x87 SpirtHac ","\xe2\x97\x87 SpirtHack ",
	"\xe2\x97\x87 SpirtHack ","\xe2\x97\x87 pirtHack ","\xe2\x97\x87 irtHack ","\xe2\x97\x87 rtHack ",
	"\xe2\x97\x87 tHack ","\xe2\x97\x87 Hack ","\xe2\x97\x87 ack ","\xe2\x97\x87 ck ","\xe2\x97\x87 k ","\xe2\x97\x87 ","\xe2\x97\x87 "
};
static const char* _Airflow[] = {
	"a","ai","air","airf","airfl","airflo","airflow",
	"airflow.","airflow.s","airflow.su","airflow.su",
	"airflow.s","airflow.","airflow","airflo","airfl",
	"airf","air","ai","a"," "
};
static const char* _Monolith[] = {
	"[M-------] ","[Mo------] ","[Mon-----] ","[Mono----] ",
	"[Mono----] ","[Monol---] ","[Monoli--] ","[Monolit-] ",
	"[Monolith] ","[Monolit-] ","[Monoli--] ","[Monol---] ",
	"[Mono----] ","[Mon-----] ","[Mo------] ","[M-------] ",
	"[--------] ","[-------<] ","[------<8] ","[-----<8>] ",
	"[----<8>-] ","[---<8>--] ","[--<8>---] ","[-<8>----] ",
	"[<8>-----] ","[8>------] ","[>-------] ","[--------] "
};
static const char* _Onetap_su[] = {
	"onetap.su","nepat.su o","epat.su on","pat.su one","ap.su onet",
	"t.su oneta",".su onetap","su onetap.","u onetap.s","onetap.su"
};
static const char* _Nemesis[] = {
	"nemesis","n3m3sis","nemesis","n3m3sis",
	"nemesis","n3m3sis","nemesis","n3m3sis"
};
static const char* _RaweTrip[] = {
	" \xe3\x80\x84 "," R>|\xe3\x80\x84 "," RA>|\xe3\x80\x84 "," R4W>|\xe3\x80\x84 "," RAW\xd0\xad>|\xe3\x80\x84 ",
	" R4W3T>|\xe3\x80\x84 "," RAW\xce\xa3TR>|\xe3\x80\x84 "," \xd0\xaf" "4WETRI>|\xe3\x80\x84 "," RAWETRIP>|\xe3\x80\x84 ",
	" RAWETRIP<|\xe3\x80\x84 "," R4WETRI<|\xe3\x80\x84 "," RAW\xce\xa3TR<|\xe3\x80\x84 "," R4W3T<|\xe3\x80\x84 ",
	" RAW\xd0\xad<|\xe3\x80\x84 "," R4W<|\xe3\x80\x84 "," RA<|\xe3\x80\x84 "," R<|\xe3\x80\x84 "," \xe3\x80\x84 "
};
static const char* _Ev0lve[] = {
	"           ","         e","        ev","       ev0","      ev0l",
	"     ev0lv","    ev0lve","   ev0lve.","  ev0lve.x "," ev0lve.xy ",
	" ev0lve.xyz "," ev0lve.xyz "," ev0lve.xyz "," ev0lve.xyz ",
	" ev0lve.xyz "," ev0lve.xyz "," ev0lve.xyz "," ev0lve.xyz ",
	" ev0lve.xyz "," ev0lve.xyz "," ev0lve.xyz ",
	" v0lve.xyz "," 0lve.xyz "," lve.xyz ","ve.xyz   ",
	"e.xyz    ",".xyz     ","xyz      ","yz       ","z        "
};
static const char* _BosniaHook[] = {
	"\xe2\x98\x82" "","\xe2\x98\x82""B","\xe2\x98\x82""Bo","\xe2\x98\x82""Bos","\xe2\x98\x82""Bosn","\xe2\x98\x82""Bosni","\xe2\x98\x82""Bosnia",
	"\xe2\x98\x82""BosniaH","\xe2\x98\x82""BosniaHo","\xe2\x98\x82""BosniaHoo","\xe2\x98\x82""BosniaHook",
	"B\xe2\x98\x82""osniaHook","Bo\xe2\x98\x82""sniaHook","Bos\xe2\x98\x82""niaHook","Bosn\xe2\x98\x82""iaHook",
	"Bosni\xe2\x98\x82""aHook","Bosnia\xe2\x98\x82""Hook","BosniaH\xe2\x98\x82""ook","BosniaHo\xe2\x98\x82""ok",
	"BosniaHoo\xe2\x98\x82""k","BosniaHook\xe2\x98\x82""","BosniaHook","BosniaHook",
	"BosniaHook","_osniaHoo_","__sniaHo__","___niaH___",
	"____ia____","__________","_____","__","_","","\xe2\x98\x82" ""
};
static const char* _MonkeySquad[] = {
	">",">M",">Mo",">Mon",">Monk",">Monke",">Monkey",">Monkey",
	">MonkeyS",">MonkeySq",">MonkeySqu",">MonkeySqua",">MonkeySquad",
	"MonkeySquad","$onkeySquad","M$nkeySquad","Mo$keySquad",
	"Mon$eySquad","Monk$ySquad","Monke$Squad","Monkey$quad",
	"MonkeyS$uad","MonkeySq$ad","MonkeySqu$d","MonkeySqua$",
	"MonkeySquad","MonkeySqua<","MonkeySqu<","MonkeySq<","MonkeyS<",
	"Monkey<","Monke<","Monk<","Mon<","Mo<","M<","<"
};
static const char* _ShanDongGanZhi[] = {
	"\xe2\x99\x9f",
	"\xe2\x99\x9f\xe5\xb1\xb1",
	"\xe2\x99\x9f\xe5\xb1\xb1\xe6\x9d\xb1",
	"\xe2\x99\x9f\xe5\xb1\xb1\xe6\x9d\xb1\xe6\x84\x9f",
	"\xe2\x99\x9f\xe5\xb1\xb1\xe6\x9d\xb1\xe6\x84\x9f\xe7\x9f\xa5",
	"\xe5\xb1\xb1\xe2\x99\x9f\xe6\x9d\xb1\xe6\x84\x9f\xe7\x9f\xa5",
	"\xe5\xb1\xb1\xe6\x9d\xb1\xe2\x99\x9f\xe6\x84\x9f\xe7\x9f\xa5",
	"\xe5\xb1\xb1\xe6\x9d\xb1\xe6\x84\x9f\xe2\x99\x9f\xe7\x9f\xa5",
	"\xe5\xb1\xb1\xe6\x9d\xb1\xe6\x84\x9f\xe7\x9f\xa5\xe2\x99\x9f",
	"\xe5\xb1\xb1\xe6\x9d\xb1\xe6\x84\x9f\xe2\x99\x9f\xe7\x9f\xa5",
	"\xe5\xb1\xb1\xe6\x9d\xb1\xe2\x99\x9f\xe6\x84\x9f\xe7\x9f\xa5",
	"\xe5\xb1\xb1\xe2\x99\x9f\xe6\x9d\xb1\xe6\x84\x9f\xe7\x9f\xa5",
	"\xe2\x99\x9f\xe5\xb1\xb1\xe6\x9d\xb1\xe6\x84\x9f\xe7\x9f\xa5",
	"_\xe5\xb1\xb1\xe6\x9d\xb1\xe6\x84\x9f\xe7\x9f\xa5_",
	"__\xe6\x9d\xb1\xe6\x84\x9f\xe7\x9f\xa5__",
	"___\xe6\x84\x9f\xe7\x9f\xa5___",
	"____\xe7\x9f\xa5____",
	"__________",
	"_____","__","_","",
	"\xe2\x99\x9f"
};

#define FRAME_COUNT(array) static_cast<int>(sizeof(array) / sizeof((array)[0]))
static Preset g_presets[] = {
	{"Aimware",         _Aimware_net,     FRAME_COUNT(_Aimware_net)},
	{"Airflow",         _Airflow,         FRAME_COUNT(_Airflow)},
	{"BosniaHook",      _BosniaHook,      FRAME_COUNT(_BosniaHook)},
	{"Ev0lve",          _Ev0lve,          FRAME_COUNT(_Ev0lve)},
	{"Fatality",        _Fatality_win,    FRAME_COUNT(_Fatality_win)},
	{"GameSense",       _Gamesense_pub,   FRAME_COUNT(_Gamesense_pub)},
	{"Iniuria",         _Iniuria_us,      FRAME_COUNT(_Iniuria_us)},
	{"Legendware",      _Legendware,      FRAME_COUNT(_Legendware)},
	{"Millionware",     _Millionware,     FRAME_COUNT(_Millionware)},
	{"MonkeySquad",     _MonkeySquad,     FRAME_COUNT(_MonkeySquad)},
	{"Monolith",        _Monolith,        FRAME_COUNT(_Monolith)},
	{"Nemesis",         _Nemesis,         FRAME_COUNT(_Nemesis)},
	{"Neverlose",       _Neverlose_cc,    FRAME_COUNT(_Neverlose_cc)},
	{"Nixware",         _Nixware_cc,      FRAME_COUNT(_Nixware_cc)},
	{"Onetap.su",       _Onetap_su,       FRAME_COUNT(_Onetap_su)},
	{"Primordial",      _Primordial_dev,  FRAME_COUNT(_Primordial_dev)},
	{"RaweTrip",        _RaweTrip,        FRAME_COUNT(_RaweTrip)},
	{"Rifk",            _Rifk,            FRAME_COUNT(_Rifk)},
	{"Skeet.cc",        _Skeet_cc,        FRAME_COUNT(_Skeet_cc)},
	{"SpirtHack",       _SpirtHack,       FRAME_COUNT(_SpirtHack)},
	{"Weave",           _Weave,           FRAME_COUNT(_Weave)},
	{"\xe5\xb1\xb1\xe6\x9d\xb1\xe6\x84\x9f\xe7\x9f\xa5", _ShanDongGanZhi, FRAME_COUNT(_ShanDongGanZhi)},
};
#undef FRAME_COUNT
static constexpr int kPresetCount = 22;

// ============================================================================
// State
// ============================================================================
static uintptr_t g_engine2Base = 0;
// ConVar patch 延迟到每帧重试,避免与其它作弊(SK)双注入时的初始化竞态
static bool g_nameConVarPatched = false;

static CNameChanger::Mode g_mode = CNameChanger::Mode::Disabled;
static char g_editText[ 256 ] = "";
static char g_customName[ 128 ] = "";
static float g_animSpeed = 0.5f;
static char g_realName[ 256 ] = "";
static bool g_nameWasChanged = false;
static bool g_lobbyRestoreIssued = false;

static int g_presetIndex = -1;
static CNameChanger::AnimStyle g_animStyle = CNameChanger::AnimStyle::Static;
static std::string g_animStr;
static bool g_radarToggle = false;
static std::chrono::steady_clock::time_point g_lastFrame;
static std::string g_lastNameSent;

// ============================================================================
// ReadRealName — from controller
// ============================================================================
static void ReadRealName()
{
	// 用 schema 运行时解析的 m_sSanitizedPlayerName,自动适配版本;
	// 之前硬编码 0x6F4 在现版本已过期,读到乱码导致注入后名字被改坏。
	auto* pController = GetCL_Players()->GetLocalPlayerController();
	if ( !pController )
		return;

	const char* s = pController->m_sSanitizedPlayerName();
	if ( s && s[ 0 ] )
		strncpy_s( g_realName, s, sizeof( g_realName ) - 1 );
}

// ============================================================================
// PatchNameConVar — byte-patch 'name' convar flags
// ============================================================================
static bool PatchNameConVar()
{
	if ( !g_engine2Base ) return false;

	// 1) Byte-patch: jnz -> jmp in ConVar registration
	const uint8_t pattern[] = { 0x48, 0xC1, 0xE8, 0x00, 0xA8, 0x01, 0x75, 0x15 };
	const uint8_t mask[] = { 0xFF, 0xFF, 0xFF, 0x00, 0xFF, 0xFF, 0xFF, 0xFF };
	for ( uintptr_t i = 0; i < 0x800000; ++i ) {
		uint8_t* p = (uint8_t*)( g_engine2Base + i );
		bool match = true;
		for ( int j = 0; j < 8; ++j )
			if ( ( p[ j ] & mask[ j ] ) != ( pattern[ j ] & mask[ j ] ) ) { match = false; break; }
		if ( !match ) continue;
		DWORD old;
		VirtualProtect( p + 6, 1, PAGE_EXECUTE_READWRITE, &old );
		p[ 6 ] = 0xEB;
		VirtualProtect( p + 6, 1, old, &old );
		break;
	}

	// 2) Modify flags of the already-registered 'name' convar
	// 用 SDK 接口替代硬编码 RVA(engine2+0x688B08 / 0x3FE8E0),
	// 避免双注入时 ResolveConVar 的竞态崩溃
	constexpr int FCVAR_DEVELOPMENTONLY = 0x2;
	constexpr int FCVAR_USERINFO = 0x400;

	auto* pCvar = SDK::Interfaces::EngineCvar();
	if ( !pCvar ) return false;

	auto* pNameConVar = pCvar->Find( "name" );
	if ( !pNameConVar ) return false;

	pNameConVar->nFlags &= ~FCVAR_DEVELOPMENTONLY;
	pNameConVar->nFlags |= FCVAR_USERINFO;
	return true;
}

// SEH 防护单独抽出来(OnFrame 里有 std::string 等需展开对象,不能直接放 __try)
static bool TryPatchNameConVar()
{
	bool ok = false;
	__try
	{
		ok = PatchNameConVar();
	}
	__except ( EXCEPTION_EXECUTE_HANDLER )
	{
		ok = false;
	}
	return ok;
}

// ============================================================================
// IsActiveMatch — use SDK interface instead of hardcoded offsets
// ============================================================================
static bool IsActiveMatch()
{
	return SDK::Interfaces::EngineToClient() && SDK::Interfaces::EngineToClient()->IsInGame();
}

// ============================================================================
// SyncFromConfig
// ============================================================================
static void SyncFromConfig()
{
	if ( !menu_state::clantagEnabled ) {
		g_mode = CNameChanger::Mode::Disabled;
		return;
	}

	strncpy_s( g_customName, menu_state::playerName, sizeof( g_customName ) - 1 );
	strncpy_s( g_editText, menu_state::customClantag, sizeof( g_editText ) - 1 );
	g_animSpeed = menu_state::clantagSpeed;

	if ( menu_state::clantagSelection < kPresetCount ) {
		g_mode = CNameChanger::Mode::Animated;
		g_presetIndex = menu_state::clantagSelection;
	} else {
		// Custom slot — animationSelection now indexes the 20-item UI list (Rotate Left = 0 → AnimStyle::RotationLR = 1)
		if ( menu_state::animatedClantag ) {
			g_mode = CNameChanger::Mode::CustomAnim;
			g_animStyle = static_cast<CNameChanger::AnimStyle>( menu_state::animationSelection + 1 );
		} else {
			g_mode = CNameChanger::Mode::Static;
		}
	}
}

// ============================================================================
// Magic string (for Minecraft mode)
// ============================================================================
static const char* kMagicChars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";

static std::string MakeMagicString( int len ) {
	static thread_local std::mt19937 rng( std::random_device{}() );
	std::uniform_int_distribution<int> dist( 0, (int)strlen( kMagicChars ) - 1 );
	std::string r; r.reserve( len );
	for ( int i = 0; i < len; ++i ) r += kMagicChars[ dist( rng ) ];
	return r;
}

// ============================================================================
// BuildCustomStyleFrame — 19 animation styles
// ============================================================================
static std::string BuildCustomStyleFrame( const std::string& text )
{
	int len = (int)text.length();
	if ( len == 0 ) return "";

	auto now = std::chrono::steady_clock::now();
	static auto lastAnim = now;
	static std::string lastText;
	static int animProgress = 0;
	static int animProgress2 = 0;
	static bool animReverse = false;
	static auto animStartTime = now;

	if ( lastText != text ) {
		lastText = text;
		animProgress = 0;
		animProgress2 = 0;
		animReverse = false;
		animStartTime = now;
	}

	bool shouldStep = std::chrono::duration<float>( now - lastAnim ).count() >= g_animSpeed;
	if ( shouldStep ) lastAnim = now;

	switch ( g_animStyle ) {
	case CNameChanger::AnimStyle::Static:
		return text;

	case CNameChanger::AnimStyle::Progressive:
		if ( shouldStep ) animProgress = ( animProgress % len ) + 1;
		return text.substr( 0, animProgress > 0 ? animProgress : 1 );

	case CNameChanger::AnimStyle::Retractable:
		if ( shouldStep ) {
			if ( !animReverse ) { animProgress++; if ( animProgress >= len ) animReverse = true; }
			else { animProgress--; if ( animProgress <= 1 ) animReverse = false; }
		}
		return text.substr( 0, (std::max)( 1, animProgress ) );

	case CNameChanger::AnimStyle::RetractableFront:
		if ( shouldStep ) {
			if ( !animReverse ) {
				animProgress++; if ( animProgress >= len ) animReverse = true;
			} else {
				animProgress2++; if ( animProgress2 >= len ) { animReverse = false; animProgress = 0; animProgress2 = 0; }
			}
		}
		if ( !animReverse ) return text.substr( 0, animProgress > 0 ? animProgress : 1 );
		{ int subLen = len - animProgress2; return text.substr( animProgress2, (std::max)( 1, subLen ) ); }

	case CNameChanger::AnimStyle::RotationLR:
		if ( shouldStep ) animProgress = ( animProgress % len ) + 1;
		{ int cut = animProgress > 0 ? animProgress : 1;
		  return text.substr( cut ) + " " + text.substr( 0, cut ); }

	case CNameChanger::AnimStyle::RotationRL:
		if ( shouldStep ) animProgress = ( animProgress % len ) + 1;
		{ int cut = animProgress > 0 ? animProgress : 1;
		  std::string second = text.substr( len - cut );
		  std::reverse( second.begin(), second.end() );
		  return second + " " + text.substr( 0, len - cut ); }

	case CNameChanger::AnimStyle::ScrollProg:
		if ( shouldStep ) {
			if ( !animReverse ) { animProgress++; if ( animProgress >= len ) animReverse = true; }
			else { animProgress2++; if ( animProgress2 >= len ) { animReverse = false; animProgress = 0; animProgress2 = 0; } }
		}
		if ( !animReverse ) return text.substr( 0, animProgress > 0 ? animProgress : 1 );
		return text.substr( len - animProgress2 ) + " " + text.substr( 0, len - animProgress2 );

	case CNameChanger::AnimStyle::PerTimePercent:
		{ float elapsed = std::chrono::duration<float>( now - animStartTime ).count();
		  int showLen = (int)( elapsed * ( 5.0f - g_animSpeed ) );
		  showLen = len > 0 ? ( showLen % len ) : 0;
		  showLen = std::clamp( showLen, 1, len );
		  return text.substr( 0, showLen ); }

	case CNameChanger::AnimStyle::Decode:
		{
			static int s_decodePhase = 1;
			static int s_decodeFrame = 0;
			static int s_decodeRestoreIdx = 0;
			static auto decodeLastAnim = now;
			static const char* kSyms = "@#$%&*!?~^<>|/[]{}=:;+-";
			static const int kSymLen = 18;

			if ( g_animStyle != CNameChanger::AnimStyle::Decode ) return text; // reset guard
			// NOTE: simplified Decode — phase 1 random → phase 2 scramble → phase 3 restore
			float phaseSpeed = ( s_decodePhase == 2 ) ? g_animSpeed * 0.3f : g_animSpeed;
			bool decodeStep = std::chrono::duration<float>( now - decodeLastAnim ).count() >= phaseSpeed;
			if ( decodeStep ) decodeLastAnim = now;

			if ( s_decodePhase == 1 ) {
				if ( decodeStep ) { s_decodeFrame++; if ( s_decodeFrame >= 15 ) { s_decodePhase = 2; s_decodeFrame = 0; } }
				std::string result = text;
				for ( int i = 0; i < 1 + ( std::rand() % 2 ); ++i )
					result[ std::rand() % len ] = kSyms[ std::rand() % kSymLen ];
				return result;
			} else if ( s_decodePhase == 2 ) {
				if ( decodeStep ) { s_decodeFrame++; if ( s_decodeFrame >= 8 ) { s_decodePhase = 3; s_decodeFrame = 0; s_decodeRestoreIdx = 0; } }
				std::string result; result.reserve( len );
				for ( int i = 0; i < len; ++i ) result += kSyms[ std::rand() % kSymLen ];
				return result;
			} else {
				if ( decodeStep ) { s_decodeRestoreIdx++; if ( s_decodeRestoreIdx >= len ) { s_decodePhase = 1; s_decodeFrame = 0; s_decodeRestoreIdx = 0; } }
				std::string result; result.reserve( len );
				for ( int i = 0; i < len; ++i )
					result += ( i < s_decodeRestoreIdx ) ? text[ i ] : kSyms[ std::rand() % kSymLen ];
				return result;
			}
		}

	case CNameChanger::AnimStyle::Typewriter:
		{ static int typeIdx = 0; static int typeHoldFrames = 0;
		  if ( shouldStep ) { if ( typeIdx < len ) typeIdx++; else { typeHoldFrames++; if ( typeHoldFrames >= 5 ) { typeIdx = 0; typeHoldFrames = 0; } } }
		  if ( typeIdx >= len ) return text;
		  return text.substr( 0, typeIdx ) + "_"; }

	case CNameChanger::AnimStyle::Glitch:
		{ static int glitchState = 0; static int glitchFrames = 0; static int normalFrames = 0;
		  if ( shouldStep ) { if ( glitchState == 0 ) { normalFrames++; if ( normalFrames >= 5 + ( std::rand() % 11 ) ) { glitchState = 1; glitchFrames = 1 + ( std::rand() % 2 ); normalFrames = 0; } }
		  else { glitchFrames--; if ( glitchFrames <= 0 ) { glitchState = 0; normalFrames = 0; } } }
		  if ( glitchState == 1 ) { static const char* ks = "@#$%&*!?~^<>|/[]{}=:;+-"; std::string r = text;
		  for ( int i = 0; i < 2 + ( std::rand() % 3 ); ++i ) r[ std::rand() % len ] = ks[ std::rand() % 18 ]; return r; }
		  return text; }

	case CNameChanger::AnimStyle::CoreDump:
		{ static int cdPhase = 0; static int cdFrame = 0; static const char* ks = "@#$%&*!?~^<>|/[]{}=:;+-";
		  if ( shouldStep ) { cdFrame++; switch ( cdPhase ) {
		  case 0: if ( cdFrame >= 4 ) { cdPhase = 1; cdFrame = 0; } break;
		  case 1: if ( cdFrame >= 2 ) { cdPhase = 2; cdFrame = 0; } break;
		  case 2: if ( cdFrame >= 3 ) { cdPhase = 3; cdFrame = 0; } break;
		  case 3: if ( cdFrame >= len ) { cdPhase = 0; cdFrame = 0; } break; } }
		  if ( cdPhase == 0 ) return text;
		  if ( cdPhase == 1 ) { std::string r; for ( int i = 0; i < len; ++i ) r += ks[ std::rand() % 18 ]; return r; }
		  if ( cdPhase == 2 ) return "[CORE DUMPED]";
		  { std::string r; for ( int i = 0; i < len; ++i ) r += ( i < cdFrame ) ? text[ i ] : ks[ std::rand() % 18 ]; return r; } }

	case CNameChanger::AnimStyle::Penetrate:
		{ static int pp = 0; static int ppPhase = 0; static const char* ks = "@#$%&*!?~^<>|/[]{}=:;+-";
		  if ( shouldStep ) { if ( ppPhase == 0 ) { pp++; if ( pp > len ) { ppPhase = 1; pp = 0; } }
		  else if ( ppPhase == 1 ) { pp++; if ( pp >= 3 ) { ppPhase = 2; pp = 0; } }
		  else { pp++; if ( pp > len ) { ppPhase = 0; pp = 0; } } }
		  std::string r = text;
		  if ( ppPhase == 0 ) { for ( int i = len - pp; i < len; ++i ) if ( i >= 0 ) r[ i ] = ks[ std::rand() % 18 ]; }
		  else if ( ppPhase == 1 ) { for ( int i = 0; i < len; ++i ) r[ i ] = ks[ std::rand() % 18 ]; }
		  else { for ( int i = pp; i < len; ++i ) r[ i ] = ks[ std::rand() % 18 ]; }
		  return r; }

	case CNameChanger::AnimStyle::PasswordLock:
		{ static int lockIdx = 0; static int lockHold = 0; static const char* kc = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789@#$%&*";
		  if ( shouldStep ) { if ( lockIdx < len ) lockIdx++; else { lockHold++; if ( lockHold >= 4 ) { lockIdx = 0; lockHold = 0; } } }
		  std::string r; for ( int i = 0; i < len; ++i ) r += ( i < lockIdx ) ? text[ i ] : kc[ std::rand() % 24 ]; return r; }

	case CNameChanger::AnimStyle::ScanLine:
		{ static int sp = 0; static int sh = 0;
		  if ( shouldStep ) { if ( sp < len ) sp++; else { sh++; if ( sh >= 3 ) { sp = 0; sh = 0; } } }
		  std::string r; for ( int i = 0; i < len; ++i ) r += ( i < sp ) ? text[ i ] : '.'; return r; }

	case CNameChanger::AnimStyle::Heart:
		{ static int hp = 0; static int hph = 0; static int hh = 0;
		  if ( shouldStep ) { if ( hph == 0 ) { hp++; if ( hp >= len ) hph = 1; }
		  else { hp--; if ( hp <= 0 ) { hh++; if ( hh >= 2 ) { hph = 0; hh = 0; } hp = 0; } } }
		  std::string r = text;
		  int pos = hph == 0 ? hp - 1 : hp;
		  if ( pos >= 0 && pos < len ) r.replace( pos, 1, "\xE2\x9D\xA4" );
		  return r; }

	case CNameChanger::AnimStyle::CmdSpinner:
		{ static int cp = 0; static int cf = 0; static auto cps = now; static const char* ks = "@#$%&*!?~^<>|/[]{}=:;+-";
		  if ( shouldStep ) { cf++; switch ( cp ) {
		  case 0: if ( cf >= 20 ) { cp = 1; cf = 0; } break;
		  case 1: if ( cf >= 6 )  { cp = 2; cf = 0; } break;
		  case 2: if ( cf >= len ) { cp = 3; cf = 0; cps = now; } break;
		  case 3: if ( std::chrono::duration<float>( now - cps ).count() >= 2.0f ) { cp = 0; cf = 0; } break; } }
		  if ( cp == 0 ) { const char* spn = "-\\|/"; char buf[ 64 ]; snprintf( buf, 64, "> [ %c ] loading", spn[ cf % 4 ] ); return buf; }
		  if ( cp == 1 ) { std::string r; for ( int i = 0; i < len; ++i ) r += ks[ std::rand() % 18 ]; return r; }
		  if ( cp == 2 ) { std::string r; for ( int i = 0; i < len; ++i ) r += ( i < cf ) ? text[ i ] : ks[ std::rand() % 18 ]; return r; }
		  return text; }

	case CNameChanger::AnimStyle::CmdLog:
		{ static int cp = 0; static int cf = 0; static auto cps = now; static const char* ks = "@#$%&*!?~^<>|/[]{}=:;+-";
		  static const char* const lines[] = { "> initializing", "> loading modules", "> connecting", "> ready" };
		  if ( shouldStep ) { cf++; switch ( cp ) {
		  case 0: if ( std::chrono::duration<float>( now - cps ).count() >= 4 * 2.0f ) { cp = 1; cf = 0; } break;
		  case 1: if ( cf >= 6 ) { cp = 2; cf = 0; } break;
		  case 2: if ( cf >= len ) { cp = 3; cf = 0; cps = now; } break;
		  case 3: if ( std::chrono::duration<float>( now - cps ).count() >= 2.0f ) { cp = 0; cf = 0; cps = now; } break; } }
		  if ( cp == 0 ) { float el = std::chrono::duration<float>( now - cps ).count();
		  int li = (std::min)( (int)( el / 2.0f ), 3 ); bool ok = ( el - li * 2.0f ) >= 1.0f;
		  return std::string( lines[ li ] ) + ( ok ? "... OK" : "_" ); }
		  if ( cp == 1 ) { std::string r; for ( int i = 0; i < len; ++i ) r += ks[ std::rand() % 18 ]; return r; }
		  if ( cp == 2 ) { std::string r; for ( int i = 0; i < len; ++i ) r += ( i < cf ) ? text[ i ] : ks[ std::rand() % 18 ]; return r; }
		  return text; }

	case CNameChanger::AnimStyle::CmdDots:
		{ static int cp = 0; static int cf = 0; static auto cps = now; static const char* ks = "@#$%&*!?~^<>|/[]{}=:;+-";
		  if ( shouldStep ) { cf++; switch ( cp ) {
		  case 0: if ( cf >= 12 ) { cp = 1; cf = 0; } break;
		  case 1: if ( cf >= 6 )  { cp = 2; cf = 0; } break;
		  case 2: if ( cf >= len ) { cp = 3; cf = 0; cps = now; } break;
		  case 3: if ( std::chrono::duration<float>( now - cps ).count() >= 2.0f ) { cp = 0; cf = 0; } break; } }
		  if ( cp == 0 ) { int dc = cf % 4; return "> loading" + std::string( dc, '.' ) + ( dc == 0 ? "   _" : "_" ); }
		  if ( cp == 1 ) { std::string r; for ( int i = 0; i < len; ++i ) r += ks[ std::rand() % 18 ]; return r; }
		  if ( cp == 2 ) { std::string r; for ( int i = 0; i < len; ++i ) r += ( i < cf ) ? text[ i ] : ks[ std::rand() % 18 ]; return r; }
		  return text; }

	case CNameChanger::AnimStyle::NetError:
		{ static int np = 0; static int nf = 0; static auto nps = now; static const char* ks = "@#$%&*!?~^<>|/[]";
		  if ( shouldStep ) { nf++; switch ( np ) {
		  case 0: if ( nf >= 22 ) { np = 1; nf = 0; } break;
		  case 1: if ( nf >= 22 ) { np = 2; nf = 0; } break;
		  case 2: if ( nf >= 2 )  { np = 3; nf = 0; } break;
		  case 3: if ( nf >= len ) { np = 4; nf = 0; nps = now; } break;
		  case 4: if ( std::chrono::duration<float>( now - nps ).count() >= 2.0f ) { np = 0; nf = 0; } break; } }
		  if ( np == 0 ) { int f = nf % 22; if ( f < 3 ) return "OK"; if ( f < 6 ) return "Moved"; if ( f < 9 ) return "Forbidden";
		  if ( f < 16 ) return "[404 NOT FOUND]"; if ( f < 19 ) return "I'm a Hacker"; return "Internal Server Error"; }
		  if ( np == 1 ) { std::string e = "Internal Server Error"; int keep = (std::max)( 0, (int)e.length() - nf );
		  return e.substr( 0, keep ) + std::string( nf, '_' ); }
		  if ( np == 2 ) { std::string r; for ( int i = 0; i < len; ++i ) r += ks[ std::rand() % 16 ]; return r; }
		  if ( np == 3 ) { std::string r; for ( int i = 0; i < len; ++i ) r += ( i < nf ) ? text[ i ] : ks[ std::rand() % 16 ]; return r; }
		  return text; }

	case CNameChanger::AnimStyle::DirBruteforce:
		{ static int dp = 0; static int df = 0; static auto dps = now; static const char* ks = "@#$%&*!?~^<>|/[]";
		  if ( shouldStep ) { df++; switch ( dp ) {
		  case 0: if ( df >= 9 ) { dp = 1; df = 0; dps = now; } break;
		  case 1: if ( std::chrono::duration<float>( now - dps ).count() >= 2.0f ) { dp = 2; df = 0; } break;
		  case 2: if ( df >= 3 ) { dp = 3; df = 0; } break;
		  case 3: if ( df >= 2 ) { dp = 4; df = 0; } break;
		  case 4: if ( df >= len ) { dp = 5; df = 0; dps = now; } break;
		  case 5: if ( std::chrono::duration<float>( now - dps ).count() >= 2.0f ) { dp = 0; df = 0; } break; } }
		  if ( dp == 0 ) { int f = df % 9; if ( f < 3 ) return "GET " + text + " -> 404";
		  if ( f < 5 ) return "POST " + text + " -> 404";
		  if ( f < 7 ) return "PUT " + text + " -> 404"; return "PATCH " + text + " -> 404"; }
		  if ( dp == 1 ) return "ALL REQUESTS FAILED 404";
		  if ( dp == 2 ) { std::string b = "ALL REQUESTS FAILED 404"; int n = (int)b.length();
		  for ( int i = 0; i < ( df + 1 ) * 4 && i < n; ++i ) b[ std::rand() % n ] = ks[ std::rand() % 16 ]; return b; }
		  if ( dp == 3 ) { std::string r; for ( int i = 0; i < len; ++i ) r += ks[ std::rand() % 16 ]; return r; }
		  if ( dp == 4 ) { std::string r; for ( int i = 0; i < len; ++i ) r += ( i < df ) ? text[ i ] : ks[ std::rand() % 16 ]; return r; }
		  return text; }

	default:
		return text;
	}
}

// ============================================================================
// RestoreRealNameOnce
// ============================================================================
static void RestoreRealNameOnce()
{
	if ( !g_realName[ 0 ] ) ReadRealName();
	if ( !g_realName[ 0 ] ) return;
	RunNameCommand( g_realName );
	g_nameWasChanged = false;
}

// ============================================================================
// Init / Shutdown
// ============================================================================
static CNameChanger g_CNameChanger{};

auto CNameChanger::Init() -> bool
{
	g_engine2Base = (uintptr_t)GetModuleHandleW( L"engine2.dll" );
	if ( !g_engine2Base ) return false;

	// ConVar patch 延迟到 OnFrame 每帧重试(见 OnFrame),避免双注入竞态崩溃
	g_lastFrame = std::chrono::steady_clock::now();
	return true;
}

auto CNameChanger::Shutdown() -> void
{
	if ( g_nameWasChanged && g_mode != Mode::Disabled && g_realName[ 0 ] ) {
		RunNameCommand( g_realName );
	}
}

auto CNameChanger::ApplyName() -> void
{
	SyncFromConfig();
	if ( g_mode == Mode::Disabled ) {
		RestoreRealNameOnce();
		return;
	}
	OnFrame();
}

auto CNameChanger::OnFrame() -> void
{
	// 延迟 + 节流重试:patch 不在 OnInit 立即做;失败后隔 1s 再试,
	// 避免双注入竞态窗口内每帧触发异常(VEH 会在 SEH 前记录,刷爆崩溃日志)
	if ( !g_nameConVarPatched )
	{
		static std::chrono::steady_clock::time_point s_lastTry{};
		const auto nowTry = std::chrono::steady_clock::now();
		if ( s_lastTry == std::chrono::steady_clock::time_point{}
			|| nowTry - s_lastTry >= std::chrono::milliseconds( 1000 ) )
		{
			s_lastTry = nowTry;
			if ( TryPatchNameConVar() )
				g_nameConVarPatched = true;
		}
	}

	SyncFromConfig();
	auto now = std::chrono::steady_clock::now();
	g_lastFrame = now;

	if ( !g_realName[ 0 ] ) ReadRealName();

	if ( !IsActiveMatch() ) {
		if ( g_nameWasChanged && !g_lobbyRestoreIssued ) {
			RestoreRealNameOnce();
			g_lobbyRestoreIssued = true;
		}
		return;
	}
	g_lobbyRestoreIssued = false;

	std::string targetName;

	switch ( g_mode ) {
	case Mode::Disabled:
		if ( g_realName[ 0 ] ) {
			if ( g_nameWasChanged )
				RestoreRealNameOnce();
			targetName = g_realName;
		}
		break;

	case Mode::Animated: {
		static auto lastAnim = now;
		if ( std::chrono::duration<float>( now - lastAnim ).count() < g_animSpeed ) break;
		lastAnim = now;
		const char* displayName = g_customName[ 0 ] ? g_customName : g_realName;
		if ( g_presetIndex >= 0 && g_presetIndex < kPresetCount ) {
			const Preset& p = g_presets[ g_presetIndex ];
			static int presetFrame = 0, prev = -1;
			if ( prev != g_presetIndex ) { prev = g_presetIndex; presetFrame = 0; }
			if ( p.count <= 0 ) break;
			presetFrame %= p.count;
			targetName = p.frames[ presetFrame ];
			targetName += " " + std::string( displayName );
			presetFrame = ( presetFrame + 1 ) % p.count;
			g_nameWasChanged = true;
		}
		break;
	}

	case Mode::Static: {
		const char* displayName = g_customName[ 0 ] ? g_customName : g_realName;
		targetName = g_editText[ 0 ] ? std::string( g_editText ) + " " + displayName : displayName;
		g_nameWasChanged = true;
		break;
	}

	case Mode::StaticRadar:
		targetName = std::string( g_editText ) + " " + g_realName;
		if ( g_radarToggle ) targetName += "\xC2\xA0";
		g_radarToggle = !g_radarToggle;
		g_nameWasChanged = true;
		break;

	case Mode::Minecraft:
		targetName = MakeMagicString( 10 + ( std::rand() % 7 ) );
		g_nameWasChanged = true;
		break;

	case Mode::CustomAnim: {
		static auto caLast = std::chrono::steady_clock::now();
		static std::string lastPrefix;
		auto caNow = std::chrono::steady_clock::now();
		bool isFast = ( g_animStyle >= AnimStyle::CmdSpinner && g_animStyle <= AnimStyle::DirBruteforce )
			|| g_animStyle == AnimStyle::PerTimePercent;
		if ( isFast && std::chrono::duration<float>( caNow - caLast ).count() < 1.f / 30.f ) {
			targetName = lastPrefix.empty() ? ( g_customName[ 0 ] ? g_customName : g_realName )
				: lastPrefix + " " + ( g_customName[ 0 ] ? g_customName : g_realName );
			break;
		}
		caLast = caNow;

		const std::string prefix = BuildCustomStyleFrame( std::string( g_editText ) );
		lastPrefix = prefix;
		const char* displayName = g_customName[ 0 ] ? g_customName : g_realName;
		if ( !prefix.empty() )
			targetName = prefix + " " + ( displayName[ 0 ] ? displayName : "Player" );
		else
			targetName = displayName;
		g_nameWasChanged = true;
		break;
	}
	}

	if ( !targetName.empty() && targetName != g_lastNameSent ) {
		g_lastNameSent = targetName;
		RunNameCommand( targetName.c_str() );
	}
}

// ============================================================================
// Query
// ============================================================================
auto CNameChanger::GetPresetCount() -> int { return kPresetCount; }

auto CNameChanger::GetPresetName( int idx ) -> const char*
{
	return ( idx >= 0 && idx < kPresetCount ) ? g_presets[ idx ].name : "Custom";
}

auto CNameChanger::GetAnimStyleCount() -> int { return 20; }

auto CNameChanger::GetAnimStyleName( int idx ) -> const char*
{
	static const char* names[] = {
		nullptr,     // 0 — Static, hidden from UI
		"Rotate Left", "Rotate Right", "Progressive", "Retract", "Front Retract",
		"Scroll", "Time Percent", "Decode", "Typewriter", "Glitch", "Core Dump", "Penetrate",
		"Password Lock", "Scanline", "Heart", "CMD Spinner", "CMD Log", "CMD Dots",
		"Network Error", "Directory Brute Force"
	};
	if ( idx < 0 || idx >= 21 ) return nullptr;
	return names[ idx ];
}

auto GetNameChanger() -> CNameChanger*
{
	return &g_CNameChanger;
}
