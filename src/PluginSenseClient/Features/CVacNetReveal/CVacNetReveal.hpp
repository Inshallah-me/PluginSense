#pragma once

#include <Common/Common.hpp>

#include <cstdint>
#include <cstring>
#include <chrono>
#include <vector>

// ============================================================================
// VacNetReveal — reveal the hidden VacNet navbar button in the CS2 main menu.
//
// CS2 ships a VacNet button in the main menu navbar, its icon and its tooltip.
// The compiled stylesheet hides it behind a single class:
//     #VacNet                    { visibility: collapse; }
//     .show-vacnet-link #VacNet  { visibility: visible;  }
// and mainmenu.js only grants that class to accounts with reviewer access.
//
// This unhides the button locally by tagging the panel ancestor chain with
// "show-vacnet-link". It is a local UI unhide only — whether the portal lets
// you in is decided server-side by your account.
//
// BUILD-SPECIFIC VALUES (verified against CS2 build 14174). Re-check after a
// game update:
//   OFF_MAINMENUPANEL            client.dll RVA
//   OFF_PANEL_*                  CUIPanel field offsets
//   VTABLE_FINDCHILDTRAVERSE     vtable +0x178
//   VTABLE_SETHASCLASS           vtable +0x500
//   kMakeSymbolPattern           panorama.dll byte pattern
// The parent-field offset is NOT hardcoded — it is discovered at runtime.
// ============================================================================

namespace VacNetReveal
{
	namespace detail
	{
		// -------------------------------------------------------------------
		// Scanners
		// -------------------------------------------------------------------
		inline uint8_t* FindPattern( const wchar_t* moduleName , const char* pattern )
		{
			HMODULE mod = GetModuleHandleW( moduleName );
			if ( !mod )
				return nullptr;

			auto* dos = (IMAGE_DOS_HEADER*)mod;
			auto* nt = (IMAGE_NT_HEADERS*)( (uint8_t*)mod + dos->e_lfanew );
			auto* sec = IMAGE_FIRST_SECTION( nt );

			std::vector<uint8_t> bytes, mask;
			for ( const char* p = pattern; *p; )
			{
				while ( *p == ' ' ) ++p;
				if ( !*p ) break;
				if ( p[0] == '?' )
				{
					bytes.push_back( 0 ); mask.push_back( 0 );
					p += ( p[1] == '?' ) ? 2 : 1;
				}
				else
				{
					char buf[3] = { p[0], p[1] ? p[1] : '0', 0 };
					bytes.push_back( (uint8_t)strtoul( buf , nullptr , 16 ) );
					mask.push_back( 0xFF );
					p += 2;
				}
			}

			for ( WORD s = 0; s < nt->FileHeader.NumberOfSections; ++s )
			{
				if ( !( sec[s].Characteristics & IMAGE_SCN_MEM_EXECUTE ) ) continue;
				auto* start = (uint8_t*)mod + sec[s].VirtualAddress;
				auto* end = start + sec[s].Misc.VirtualSize - bytes.size();
				for ( auto* cur = start; cur < end; ++cur )
				{
					bool match = true;
					for ( size_t i = 0; i < bytes.size(); ++i )
						if ( ( cur[i] & mask[i] ) != ( bytes[i] & mask[i] ) ) { match = false; break; }
					if ( match ) return cur;
				}
			}
			return nullptr;
		}

		// Locate a function by a string literal it references: find the string,
		// find the `lea reg,[rip+disp32]` that loads it, then walk back through
		// the int3 padding to the function start.
		inline uint8_t* FindStringRef( const wchar_t* moduleName , const char* str )
		{
			HMODULE mod = GetModuleHandleW( moduleName );
			if ( !mod )
				return nullptr;

			auto* dos = (IMAGE_DOS_HEADER*)mod;
			auto* nt = (IMAGE_NT_HEADERS*)( (uint8_t*)mod + dos->e_lfanew );
			auto* sec = IMAGE_FIRST_SECTION( nt );
			auto* base = (uint8_t*)mod;

			const size_t imageSize = nt->OptionalHeader.SizeOfImage;
			const size_t len = strlen( str );

			uint8_t* target = nullptr;
			for ( size_t i = 0; i + len + 1 < imageSize; ++i )
			{
				if ( base[i] != (uint8_t)str[0] ) continue;
				if ( memcmp( base + i , str , len ) == 0 && base[i + len] == '\0' )
				{
					target = base + i;
					break;
				}
			}
			if ( !target )
				return nullptr;

			for ( WORD s = 0; s < nt->FileHeader.NumberOfSections; ++s )
			{
				if ( !( sec[s].Characteristics & IMAGE_SCN_MEM_EXECUTE ) ) continue;
				auto* start = base + sec[s].VirtualAddress;
				auto* end = start + sec[s].Misc.VirtualSize - 7;

				for ( auto* cur = start; cur < end; ++cur )
				{
					if ( ( cur[0] & 0xF8 ) != 0x48 || cur[1] != 0x8D ) continue;
					if ( ( cur[2] & 0xC7 ) != 0x05 ) continue;          // rip-relative
					if ( cur + 7 + *(int32_t*)( cur + 3 ) != target ) continue;

					for ( auto* p = cur; p > start; --p )
						if ( p[-1] == 0xCC && p[-2] == 0xCC ) return p;
					return nullptr;
				}
			}
			return nullptr;
		}

		// -------------------------------------------------------------------
		// Offsets and signatures
		// -------------------------------------------------------------------
		static constexpr uintptr_t OFF_MAINMENUPANEL = 0x240CD78;   // client.dll

		static constexpr uintptr_t OFF_PANEL_CLASSCOUNT = 0x148;
		static constexpr uintptr_t OFF_PANEL_CLASSDATA = 0x150;
		static constexpr uintptr_t OFF_PANEL_CHILDCOUNT = 0x28;
		static constexpr uintptr_t OFF_PANEL_CHILDDATA = 0x30;

		static constexpr int VTABLE_FINDCHILDTRAVERSE = 0x178 / 8;
		static constexpr int VTABLE_SETHASCLASS = 0x500 / 8;

		static const char* kMakeSymbolPattern = "40 55 56 48 83 EC ? 48 63";
		static const char* kAddClassesAnchor = "CUIPanel::AddClassesInternal";
		static const char* kRemoveClassesAnchor = "CUIPanel::RemoveClasses";
		static const char* kVacNetClass = "show-vacnet-link";
		static const char* kVacNetPanelId = "VacNet";

		static constexpr uint16_t kInvalidSymbol = 0xFFFF;   // NOT 0
		static constexpr int kClassSymbolPool = 0;

		struct SymbolSpan
		{
			const uint16_t* data;
			size_t count;
		};

		using MakeSymbolFn = uint16_t( __fastcall* )( void* /*ignored*/ , int pool , const char* str );
		using FindChildFn = void*( __fastcall* )( void* , const char* );
		using AddClassesFn = void( __fastcall* )( void* , const SymbolSpan* , bool );
		using RemoveClassesFn = void( __fastcall* )( void* , const SymbolSpan* );
		using SetHasClassFn = void( __fastcall* )( void* , const char* , bool );

		inline void* g_menuSlot = nullptr;
		inline void* g_makeSymbol = nullptr;
		inline void* g_addClasses = nullptr;
		inline void* g_removeClasses = nullptr;
		inline uint16_t g_symbol = kInvalidSymbol;
		inline int g_parentOffset = -1;
		inline uintptr_t g_panoLo = 0;
		inline uintptr_t g_panoHi = 0;

		// -------------------------------------------------------------------
		inline void* ReadMenuGlobal()
		{
			if ( !g_menuSlot )
				return nullptr;
			__try
			{
				void* mgr = *(void**)g_menuSlot;
				return ( (uintptr_t)mgr >= 0x10000 ) ? mgr : nullptr;
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) { return nullptr; }
		}

		// Structural screen, run before any virtual call is made on a candidate.
		inline bool LooksLikePanel( void* obj )
		{
			if ( (uintptr_t)obj < 0x10000 ) return false;
			__try
			{
				const uintptr_t vt = *(uintptr_t*)obj;
				if ( vt < g_panoLo || vt >= g_panoHi ) return false;

				const int classCount = *(int*)( (uintptr_t)obj + OFF_PANEL_CLASSCOUNT );
				if ( classCount < 0 || classCount > 256 ) return false;

				const int childCount = *(int*)( (uintptr_t)obj + OFF_PANEL_CHILDCOUNT );
				if ( childCount < 0 || childCount > 4096 ) return false;

				const void* kids = *(void**)( (uintptr_t)obj + OFF_PANEL_CHILDDATA );
				if ( childCount > 0 && (uintptr_t)kids < 0x10000 ) return false;
				return true;
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) { return false; }
		}

		inline void* FindChild( void* panel , const char* id )
		{
			__try
			{
				void** vt = *(void***)panel;
				void* found = ( (FindChildFn)vt[VTABLE_FINDCHILDTRAVERSE] )( panel , id );
				return ( (uintptr_t)found >= 0x10000 ) ? found : nullptr;
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) { return nullptr; }
		}

		inline bool PanelHasClass( void* panel , uint16_t sym )
		{
			__try
			{
				const int count = *(int*)( (uintptr_t)panel + OFF_PANEL_CLASSCOUNT );
				const uint16_t* data = *(const uint16_t**)( (uintptr_t)panel + OFF_PANEL_CLASSDATA );
				if ( count <= 0 || count > 256 || (uintptr_t)data < 0x10000 ) return false;
				for ( int i = 0; i < count; ++i )
					if ( data[i] == sym ) return true;
				return false;
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) { return false; }
		}

		// Slot 0x500/8 is a long way down a vtable — make sure the entry is
		// committed executable memory before jumping to it.
		inline bool SlotIsCode( void* panel , int slot )
		{
			__try
			{
				void** vt = *(void***)panel;
				const uintptr_t entry = (uintptr_t)vt[slot];
				if ( entry < 0x10000 ) return false;

				MEMORY_BASIC_INFORMATION mbi{};
				if ( !VirtualQuery( (void*)entry , &mbi , sizeof( mbi ) ) ) return false;
				if ( mbi.State != MEM_COMMIT ) return false;

				const DWORD exec = PAGE_EXECUTE | PAGE_EXECUTE_READ |
					PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY;
				return ( mbi.Protect & exec ) != 0;
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) { return false; }
		}

			// Both calls, in this order. Neither is sufficient alone.
			// a3 = false so AddClassesInternal runs MarkStylesDirty: without it the
			// symbol lands in the class list but the stylesheet is not re-evaluated,
			// so a re-enable after RemoveClasses would not re-show the button until
			// some other interaction forces a global style pass.
			inline bool ApplyClass( void* panel )
			{
				bool ok = false;

				const SymbolSpan span{ &g_symbol, 1 };
				__try
				{
					( (AddClassesFn)g_addClasses )( panel , &span , false );
					ok = true;
				}
				__except ( EXCEPTION_EXECUTE_HANDLER ) {}

				if ( SlotIsCode( panel , VTABLE_SETHASCLASS ) )
				{
					__try
					{
						void** vt = *(void***)panel;
						( (SetHasClassFn)vt[VTABLE_SETHASCLASS] )( panel , kVacNetClass , true );
						ok = true;
					}
					__except ( EXCEPTION_EXECUTE_HANDLER ) {}
				}
				return ok;
			}

			// CUIPanel::RemoveClasses removes the symbol from both class lists and
			// marks styles dirty, so the hidden rule is re-evaluated and the button
			// goes away. Idempotent for symbols the panel does not carry.
			inline bool RemoveClass( void* panel )
			{
				if ( !panel || !g_removeClasses || g_symbol == kInvalidSymbol )
					return false;

				const SymbolSpan span{ &g_symbol, 1 };
				__try
				{
					( (RemoveClassesFn)g_removeClasses )( panel , &span );
					return true;
				}
				__except ( EXCEPTION_EXECUTE_HANDLER ) { return false; }
			}

		inline bool ChildrenContain( void* parent , void* child )
		{
			__try
			{
				const int count = *(int*)( (uintptr_t)parent + OFF_PANEL_CHILDCOUNT );
				void** kids = *(void***)( (uintptr_t)parent + OFF_PANEL_CHILDDATA );
				if ( count <= 0 || count > 4096 || (uintptr_t)kids < 0x10000 ) return false;
				for ( int i = 0; i < count; ++i )
					if ( kids[i] == child ) return true;
				return false;
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) { return false; }
		}

		// The parent field offset is discovered, not hardcoded: the pointer that
		// both looks like a panel and actually lists `panel` among its children
		// can only be the parent, so the relationship verifies itself.
		inline void* ParentOf( void* panel )
		{
			if ( !panel ) return nullptr;

			if ( g_parentOffset >= 0 )
			{
				__try
				{
					void* p = *(void**)( (uintptr_t)panel + g_parentOffset );
					return ( LooksLikePanel( p ) && ChildrenContain( p , panel ) ) ? p : nullptr;
				}
				__except ( EXCEPTION_EXECUTE_HANDLER ) { return nullptr; }
			}

			for ( int off = 0; off <= 0x100; off += 8 )
			{
				void* p = nullptr;
				__try { p = *(void**)( (uintptr_t)panel + off ); }
				__except ( EXCEPTION_EXECUTE_HANDLER ) { continue; }
				if ( !LooksLikePanel( p ) || !ChildrenContain( p , panel ) ) continue;
				g_parentOffset = off;
				return p;
			}
			return nullptr;
		}

		// Find the member of the manager struct that can reach #VacNet.
		inline void* FindAncestor( void* mgr , void** outVacNet )
		{
			*outVacNet = nullptr;

			if ( LooksLikePanel( mgr ) )
				if ( void* btn = FindChild( mgr , kVacNetPanelId ) ) { *outVacNet = btn; return mgr; }

			for ( int off = 0x00; off <= 0x60; off += 8 )
			{
				void* member = nullptr;
				__try { member = *(void**)( (uintptr_t)mgr + off ); }
				__except ( EXCEPTION_EXECUTE_HANDLER ) { continue; }
				if ( member == mgr || !LooksLikePanel( member ) ) continue;

				if ( void* btn = FindChild( member , kVacNetPanelId ) ) { *outVacNet = btn; return member; }
			}
			return nullptr;
		}

		inline bool ResolveSymbol()
		{
			if ( g_symbol != kInvalidSymbol ) return true;
			__try
			{
				g_symbol = ( (MakeSymbolFn)g_makeSymbol )( nullptr , kClassSymbolPool , kVacNetClass );
			}
			__except ( EXCEPTION_EXECUTE_HANDLER ) { g_symbol = kInvalidSymbol; }
			return g_symbol != kInvalidSymbol;
		}
	} // namespace detail

	// -----------------------------------------------------------------------
	// Public API
	// -----------------------------------------------------------------------
	inline bool Init()
	{
		using namespace detail;

		if ( HMODULE client = GetModuleHandleW( L"client.dll" ) )
			g_menuSlot = (void*)( (uintptr_t)client + OFF_MAINMENUPANEL );

		if ( HMODULE pano = GetModuleHandleW( L"panorama.dll" ) )
		{
			auto* dos = (IMAGE_DOS_HEADER*)pano;
			auto* nt = (IMAGE_NT_HEADERS*)( (uint8_t*)pano + dos->e_lfanew );
			g_panoLo = (uintptr_t)pano;
			g_panoHi = g_panoLo + nt->OptionalHeader.SizeOfImage;
		}

		g_makeSymbol = FindPattern( L"panorama.dll" , kMakeSymbolPattern );
		g_addClasses = FindStringRef( L"panorama.dll" , kAddClassesAnchor );
		g_removeClasses = FindStringRef( L"panorama.dll" , kRemoveClassesAnchor );

		return g_menuSlot && g_makeSymbol && g_addClasses && g_removeClasses && g_panoHi;
	}

	// Call every frame. Re-applies on a timer rather than once: the main menu is
	// torn down and rebuilt (leaving a match, for one) and a fresh panel tree
	// does not carry the class, so a one-shot apply silently stops working.
	// When disabled we walk the same chain and RemoveClasses instead, which
	// strips the symbol from both class lists and re-evaluates the stylesheet,
	// hiding the button again.
	inline void OnFrame( bool enabled )
	{
		using namespace detail;

		if ( !g_makeSymbol || !g_addClasses || !g_removeClasses ) return;

		const auto now = std::chrono::steady_clock::now();
		static auto s_last = now - std::chrono::seconds( 10 );
		static bool s_lastEnabled = false;

		// Toggling the switch must take effect immediately; the 1s throttle only
		// applies while the state is unchanged (panel rebuilt, re-apply/re-strip).
		const bool stateChanged = ( enabled != s_lastEnabled );
		s_lastEnabled = enabled;

		if ( !stateChanged && std::chrono::duration<float>( now - s_last ).count() < 1.0f )
			return;
		s_last = now;

		void* mgr = ReadMenuGlobal();
		if ( !mgr ) return;                       // not in the main menu

		void* vacNet = nullptr;
		void* ancestor = FindAncestor( mgr , &vacNet );
		if ( !ancestor || !vacNet ) return;

		if ( !ResolveSymbol() ) return;

		// The whole ancestor chain plus #VacNet itself. The stylesheet resolves
		// ".show-vacnet-link #VacNet" by walking up from the button, and
		// mainmenu.js puts the class on its own context panel — not necessarily
		// the ancestor the manager struct exposes. Covering the chain removes
		// the guess; the class is inert everywhere else.
		int depth = 0;
		for ( void* node = ParentOf( vacNet ); node && depth < 32; node = ParentOf( node ), ++depth )
		{
			if ( enabled )
				ApplyClass( node );
			else
				RemoveClass( node );
		}

		if ( enabled )
		{
			ApplyClass( ancestor );
			ApplyClass( vacNet );
		}
		else
		{
			RemoveClass( ancestor );
			RemoveClass( vacNet );
		}
	}
} // namespace VacNetReveal

class CVacNetReveal final
{
public:
	auto Init() -> bool;
	auto OnFrame() -> void;
};

auto GetVacNetReveal() -> CVacNetReveal*;
