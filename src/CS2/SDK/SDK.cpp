#include "SDK.hpp"

#include <DllLauncher.hpp>
#include <Common/MemoryEngine.hpp>

#include <CS2/SDK/Interface/Interface.hpp>

#include <CS2/SDK/Interface/IEngineToClient.hpp>
#include <CS2/SDK/Interface/CShemaSystemSDK.hpp>
#include <CS2/SDK/Interface/CSource2Client.hpp>
#include <CS2/SDK/Interface/CLocalize.hpp>
#include <CS2/SDK/Interface/CSoundOpSystem.hpp>
#include <CS2/SDK/Interface/IBaseFileSystem.hpp>
#include <CS2/SDK/Interface/CMaterialSystem2.hpp>
#include <CS2/SDK/Interface/IEngineCvar.hpp>
#include <CS2/SDK/Interface/CInputSystem.hpp>

#define INCLUDE_CS2_SEARCH_FUNCTION(Interface,FuncName)\
if ( !##Interface##_Search::##FuncName##Fn.Search() )\
	bIsReady = false;

namespace
{
	// 模块镜像范围(base / 结束地址):校验解析结果确实落在目标模块内
	bool GetModuleImageRange( const char* szModuleName , uintptr_t& Base , uintptr_t& End )
	{
		const auto Module = reinterpret_cast< uintptr_t >( GetModuleHandleA( szModuleName ) );
		if ( !Module )
			return false;

		const auto pDosHeader = reinterpret_cast< const IMAGE_DOS_HEADER* >( Module );
		if ( pDosHeader->e_magic != IMAGE_DOS_SIGNATURE )
			return false;

		const auto pNtHeader = reinterpret_cast< const IMAGE_NT_HEADERS* >( Module + pDosHeader->e_lfanew );
		if ( pNtHeader->Signature != IMAGE_NT_SIGNATURE )
			return false;

		Base = Module;
		End = Module + pNtHeader->OptionalHeader.SizeOfImage;

		return true;
	}

	// 从 RIP 相对指令解目标地址:位移存放处 + 后继指令地址 + disp(带符号)
	uintptr_t ResolveRipRef( uintptr_t DisplacementAddress , uintptr_t NextInstruction )
	{
		const auto Displacement = *reinterpret_cast< const std::int32_t* >( DisplacementAddress );

		return NextInstruction + static_cast< uintptr_t >( static_cast< std::intptr_t >( Displacement ) );
	}
}

namespace SDK
{
	IVEngineToClient* Interfaces::g_pEngineToClient = nullptr;
	CGameEntitySystem* Interfaces::g_pGameEntitySystem = nullptr;
	CSchemaSystem* Interfaces::g_pSchemaSystem = nullptr;
	CSource2Client* Interfaces::g_pSource2Client = nullptr;
	CLocalize* Interfaces::g_pLocalize = nullptr;
	CSoundOpSystem* Interfaces::g_pSoundOpSystem = nullptr;
	IBaseFileSystem* Interfaces::g_pBaseFileSystem = nullptr;
	CMaterialSystem2* Interfaces::g_pMaterialSystem2 = nullptr;
	IEngineCVar* Interfaces::g_pEngineCvar = nullptr;
		CInputSystem* Interfaces::g_pInputSystem = nullptr;
		void* Interfaces::g_pResourceSystem = nullptr;

	CGlobalVarsBase** Pointers::g_ppCGlobalVarsBase = nullptr;
	IVPhysics2World** Pointers::g_ppIVPhysics2World = nullptr;
	CUserCmd** Pointers::g_ppCUserCmd = nullptr;
	void** Pointers::g_ppParticleManager = nullptr;
	void** Pointers::g_ppGameRules = nullptr;
	void** Pointers::g_ppEntityList = nullptr;
	uintptr_t Pointers::g_NetworkMessages = 0;
	void** Pointers::g_ppNetworkGameClient = nullptr;

	IVEngineToClient* Interfaces::EngineToClient()
	{
		if ( !g_pEngineToClient )
		{
			CreateInterfaceFn pfnFactory = CaptureFactory( ENGINE2_DLL );
			g_pEngineToClient = CaptureInterface<IVEngineToClient>( pfnFactory , XorStr( IVENGINE_TO_CLIENT_INTERFACE_VERSION ) );

			bool bIsReady = true;

			INCLUDE_CS2_SEARCH_FUNCTION( IVEngineToClient , IsInGame );
			INCLUDE_CS2_SEARCH_FUNCTION( IVEngineToClient , GetLevelName );
			INCLUDE_CS2_SEARCH_FUNCTION( IVEngineToClient , GetLevelNameShort );

			if ( !bIsReady )
				return nullptr;
		}

		return g_pEngineToClient;
	}

	CGameEntitySystem* Interfaces::GameEntitySystem()
	{
		if ( !g_pGameEntitySystem )
		{
			/*
			00007FFBA0D9B6E | E8 817AEBFF              | call client.7FFBA0C53170                          | CGameEntitySystem->GetHighestEntityIndex
			00007FFBA0D9B6E | 8B08                     | mov ecx,dword ptr ds:[rax]                        |
			00007FFBA0D9B6F | FFC1                     | inc ecx                                           |
			00007FFBA0D9B6F | 85C9                     | test ecx,ecx                                      |
			00007FFBA0D9B6F | 0F8E FC000000            | jle client.7FFBA0D9B7F7                           |
			00007FFBA0D9B6F | 48:89BC24 40020000       | mov qword ptr ss:[rsp+0x240],rdi                  |
			00007FFBA0D9B70 | 0F1F40 00                | nop dword ptr ds:[rax],eax                        |
			00007FFBA0D9B70 | 66:0F1F8400 00000000     | nop word ptr ds:[rax+rax],ax                      |
			00007FFB90DBB71 | 48:8B0D 8968F500         | mov rcx,qword ptr ds:[0x7FFB91D11FA0]             | ppCGameEntitySystem
			00007FFB90DBB71 | 8BD3                     | mov edx,ebx                                       |
			00007FFB90DBB71 | E8 E22FECFF              | call client.7FFB90C7E700                          | CGameEntitySystem->GetBaseEntity
			00007FFB90DBB71 | 48:8BF8                  | mov rdi,rax                                       |
			00007FFB90DBB72 | 48:85C0                  | test rax,rax                                      |
			00007FFB90DBB72 | 74 76                    | je client.7FFB90DBB79C                            |
			00007FFB90DBB72 | C64424 30 00             | mov byte ptr ss:[rsp+0x30],0x0                    |
			00007FFB90DBB72 | 48:8BC8                  | mov rcx,rax                                       |
			00007FFB90DBB72 | 48:8B10                  | mov rdx,qword ptr ds:[rax]                        |
			00007FFB90DBB73 | FF92 20010000            | call qword ptr ds:[rdx+0x120]                     |
			00007FFB90DBB73 | 4C:8D05 6AAD9500         | lea r8,qword ptr ds:[0x7FFB917164A8]              | 00007FFB917164A8:"'%s'"
			00007FFB90DBB73 | BA 00010000              | mov edx,0x100                                     |
			00007FFB90DBB74 | 48:8D8C24 30010000       | lea rcx,qword ptr ss:[rsp+0x130]                  |
			00007FFB90DBB74 | 4C:8B48 08               | mov r9,qword ptr ds:[rax+0x8]                     |
			00007FFB90DBB74 | FF15 7B2A9100            | call qword ptr ds:[<V_snprintf>]                  |
			00007FFB90DBB75 | 48:8B47 10               | mov rax,qword ptr ds:[rdi+0x10]                   |
			00007FFB90DBB75 | 48:8D15 20199200         | lea rdx,qword ptr ds:[0x7FFB916DD080]             |
			00007FFB90DBB76 | 4C:8D8C24 30010000       | lea r9,qword ptr ss:[rsp+0x130]                   |
			00007FFB90DBB76 | 4C:8D4424 30             | lea r8,qword ptr ss:[rsp+0x30]                    |
			00007FFB90DBB76 | 48:8B48 18               | mov rcx,qword ptr ds:[rax+0x18]                   |
			00007FFB90DBB77 | 48:8D05 08199200         | lea rax,qword ptr ds:[0x7FFB916DD080]             |
			00007FFB90DBB77 | 48:85C9                  | test rcx,rcx                                      |
			00007FFB90DBB77 | 48:0F45D1                | cmovne rdx,rcx                                    |
			00007FFB90DBB77 | 48:8D0D 92E8A800         | lea rcx,qword ptr ds:[0x7FFB9184A018]             | 00007FFB9184A018:"Ent %3d: %s class %s name %s\n"
			*/

			auto ppGameEntitySystem = reinterpret_cast<uintptr_t>( FindPattern( CLIENT_DLL , XorStr( "48 8B 0D ? ? ? ? 8B D3 E8 ? ? ? ? 48 8B F0" ) ) );

			if ( !ppGameEntitySystem )
				return nullptr;

GetGameEntitySystemPointer:;

			g_pGameEntitySystem = *GetPtrAddress<CGameEntitySystem**>( ppGameEntitySystem );

			if ( !g_pGameEntitySystem )
			{
				Sleep( 500 );
				goto GetGameEntitySystemPointer;
			}
		}

		return g_pGameEntitySystem;
	}

	CSchemaSystem* Interfaces::SchemaSystem()
	{
		if ( !g_pSchemaSystem )
		{
			CreateInterfaceFn pfnFactory = CaptureFactory( SCHEMASYSTEM_DLL );
			g_pSchemaSystem = CaptureInterface<CSchemaSystem>( pfnFactory , XorStr( SCHEMA_SYSTEM_INTERFACE_VERSION ) );

			bool bIsReady = true;

			INCLUDE_CS2_SEARCH_FUNCTION( CSchemaSystem , GetAllTypeScope );

			if ( !bIsReady )
				return nullptr;
		}

		return g_pSchemaSystem;
	}

	CSource2Client* Interfaces::Source2Client()
	{
		if ( !g_pSource2Client )
		{
			CreateInterfaceFn pfnFactory = CaptureFactory( CLIENT_DLL );
			g_pSource2Client = CaptureInterface<CSource2Client>( pfnFactory , XorStr( SOURCE2_CLIENT_INTERFACE_VERSION ) );

			bool bIsReady = true;

			INCLUDE_CS2_SEARCH_FUNCTION( CSource2Client , GetEconItemSystem );

			if ( !bIsReady )
				return nullptr;
		}

		return g_pSource2Client;
	}

	CLocalize* Interfaces::Localize()
	{
		if ( !g_pLocalize )
		{
			CreateInterfaceFn pfnFactory = CaptureFactory( LOCALIZE_DLL );
			g_pLocalize = CaptureInterface<CLocalize>( pfnFactory , XorStr( LOCALIZE_INTERFACE_VERSION ) );

			bool bIsReady = true;

			INCLUDE_CS2_SEARCH_FUNCTION( CLocalize , FindSafe );

			if ( !bIsReady )
				return nullptr;
		}

		return g_pLocalize;
	}

	CSoundOpSystem* Interfaces::SoundOpSystem()
	{
		if ( !g_pSoundOpSystem )
		{
			CreateInterfaceFn pfnFactory = CaptureFactory( SOUNDSYSTEM_DLL );
			g_pSoundOpSystem = CaptureInterface<CSoundOpSystem>( pfnFactory , XorStr( INTERFACE_SOUNDOPSYSTEM ) );
		}

		return g_pSoundOpSystem;
	}

	IBaseFileSystem* Interfaces::BaseFileSystem()
	{
		if ( !g_pBaseFileSystem )
		{
			CreateInterfaceFn pfnFactory = CaptureFactory( FILESYSTEM_STDIO_DLL );
			g_pBaseFileSystem = CaptureInterface<IBaseFileSystem>( pfnFactory , XorStr( FILE_SYSTEM_INTERFACE_VERSION ) );
		}

		return g_pBaseFileSystem;
	}

	CMaterialSystem2* Interfaces::MaterialSystem2()
	{
		if ( !g_pMaterialSystem2 )
		{
			CreateInterfaceFn pfnFactory = CaptureFactory( MATERIALSYSTEM2_DLL );
			g_pMaterialSystem2 = CaptureInterface<CMaterialSystem2>( pfnFactory , XorStr( MATERIAL_SYSTEM2_INTERFACE_VERSION ) );

			bool bIsReady = true;

			INCLUDE_CS2_SEARCH_FUNCTION( CMaterialSystem2 , CreateMaterial );

			if ( !bIsReady )
				return nullptr;
		}

		return g_pMaterialSystem2;
	}

	IEngineCVar* Interfaces::EngineCvar()
	{
		if ( !g_pEngineCvar )
		{
			CreateInterfaceFn pfnFactory = CaptureFactory( TIER0_DLL );
			g_pEngineCvar = CaptureInterface<IEngineCVar>( pfnFactory , XorStr( ENGINE_CVAR_INTERFACE_VERSION ) );

			bool bIsReady = true;

			INCLUDE_CS2_SEARCH_FUNCTION( IEngineCVar , GetFirstCvarIterator );
			INCLUDE_CS2_SEARCH_FUNCTION( IEngineCVar , GetNextCvarIterator );
			INCLUDE_CS2_SEARCH_FUNCTION( IEngineCVar , FindVarByIndex );

			if ( !bIsReady )
				return nullptr;
		}

		return g_pEngineCvar;
	}

	CInputSystem* Interfaces::InputSystem()
	{
		if ( !g_pInputSystem )
		{
			CreateInterfaceFn pfnFactory = CaptureFactory( INPUTSYSTEM_DLL );
			g_pInputSystem = CaptureInterface<CInputSystem>( pfnFactory , XorStr( INPUT_SYSTEM_INTERFACE_VERSION ) );
		}

		return g_pInputSystem;
	}

	void* Interfaces::ResourceSystem()
	{
		if ( !g_pResourceSystem )
		{
			CreateInterfaceFn pfnFactory = CaptureFactory( RESOURCESYSTEM_DLL );
			g_pResourceSystem = CaptureInterface<void>( pfnFactory, XorStr( "ResourceSystem013" ) );
		}

		return g_pResourceSystem;
	}

	auto Pointers::GlobalVarsBase() -> CGlobalVarsBase*
	{
		if ( !g_ppCGlobalVarsBase )
		{
			auto ppCGlobalVarsBase = reinterpret_cast<uintptr_t>( FindPattern( CLIENT_DLL , XorStr( "48 8B ? ? ? ? ? 8B 48 04 FF C1 89 8B 80 06 00 00 48 8B CB E8 ? ? ? ? F3 0F 10 ? ? ? ? ? 48 8D 4C 24 60 E8 ? ? ? ? 45 8B CE 48 8D ? ? ? ? ? 48 8B CB 44 8B 00" ) ) );

			if ( !ppCGlobalVarsBase )
				return nullptr;

			g_ppCGlobalVarsBase = GetPtrAddress<CGlobalVarsBase**>( ppCGlobalVarsBase );
		}

		return *g_ppCGlobalVarsBase;
	}

	auto Pointers::CVPhys2World() -> IVPhysics2World**
	{
		if ( !g_ppIVPhysics2World )
		{
			auto ppIVPhysics2World = reinterpret_cast<uintptr_t>( FindPattern( CLIENT_DLL , XorStr( "48 8B 1D ? ? ? ? 48 8B 01 FF 90 ? ? ? ? 4C 8B 0B 4C 8D 44 24 ? 48 8B C8" ) ) );

			if ( !ppIVPhysics2World )
				return nullptr;

			g_ppIVPhysics2World = *GetPtrAddress<IVPhysics2World***>( ppIVPhysics2World );
		}

		return g_ppIVPhysics2World;
	}

	// 48 8B 0D ? ? ? ? E8 ? ? ? ? 48 8B CF 4C 8B F8
	/*
	00007FFC70ED89BC | mov rcx,qword ptr ds:[0x7FFC7228C710]                                 | CUserCmd**
	00007FFC70ED89C3 | call client.7FFC70CF0BD0                                              | GetCUserCmdArray
	00007FFC70ED89C8 | mov rcx,rdi                                                           |
	00007FFC70ED89CB | mov r15,rax                                                           |
	00007FFC70ED89CE | mov r14d,dword ptr ds:[rax+0x59A8]                                    | offset SequenceNumber
	00007FFC70ED89D5 | mov edx,r14d                                                          |
	00007FFC70ED89D8 | call client.7FFC70CF0960                                              | GetUserCmdBySequenceNumber
	*/
	auto Pointers::GetFirstCUserCmdArray() -> CUserCmd**
	{
		if ( !g_ppCUserCmd )
		{
			auto ppCUserCmd = reinterpret_cast<uintptr_t>( FindPattern( CLIENT_DLL , XorStr( "48 8B 0D ? ? ? ? E8 ? ? ? ? 48 8B CF 4C 8B F8" ) ) );

			if ( !ppCUserCmd )
			{
				DEV_LOG( "[error] ppCUserCmd\n" );

				return nullptr;
			}

			g_ppCUserCmd = *GetPtrAddress<CUserCmd***>( ppCUserCmd );
		}

		return g_ppCUserCmd;
	}

	auto Pointers::ParticleManager() -> void*
	{
		if ( !g_ppParticleManager )
		{
			auto addr = reinterpret_cast<uintptr_t>( FindPattern( CLIENT_DLL, XorStr( "48 8B 35 ? ? ? ? 44 89 6C 24 ?" ) ) );
			if ( !addr )
				return nullptr;

			g_ppParticleManager = GetPtrAddress<void**>( addr );
		}

		return g_ppParticleManager ? *g_ppParticleManager : nullptr;
	}

	auto Pointers::GameRules() -> void*
	{
		if ( !g_ppGameRules )
		{
			auto addr = reinterpret_cast<uintptr_t>( FindPattern( CLIENT_DLL, XorStr( "48 8B 0D ? ? ? ? 4C 89 70 10" ) ) );
			if ( !addr )
				return nullptr;

			g_ppGameRules = GetPtrAddress<void**>( addr );
		}

		return g_ppGameRules ? *g_ppGameRules : nullptr;
	}

	// dwEntityList:mov [rip+disp], rcx; jmp; int3(vesta 同款 pattern + RIP 解析)
	// 返回实体表基址(4×512 chunk 数组的起点),不再依赖 GetHighestEntityIndex 硬编码偏移
	auto Pointers::EntityList() -> void*
	{
		if ( !g_ppEntityList )
		{
			auto addr = reinterpret_cast<uintptr_t>( FindPattern( CLIENT_DLL, XorStr( "48 89 0D ? ? ? ? E9 ? ? ? ? CC" ) ) );
			if ( !addr )
			{
				DEV_LOG( "[error] dwEntityList pattern not found\n" );
				return nullptr;
			}

			g_ppEntityList = GetPtrAddress<void**>( addr );
		}

		return g_ppEntityList ? *g_ppEntityList : nullptr;
	}

	// CNetworkMessages 是 networksystem.dll 里的静态对象,构造函数把虚表写进对象首字段:
	//   lea rax, ??_7CNetworkMessages@@6B@
	//   mov cs:g_pNetworkMessages, rax
	// 特征码锚在 lea 上;解出虚表与对象地址后用「对象首字段 == 虚表」自校验。
	// 解析只做一次(含失败):特征码失配时不能每帧重扫整个模块,否则会持续掉帧。
	auto Pointers::NetworkMessages() -> void*
	{
		static bool bResolveAttempted = false;

		if ( !bResolveAttempted )
		{
			bResolveAttempted = true;

			const auto RefSite = reinterpret_cast< uintptr_t >( FindPattern( NETWORKSYSTEM_DLL , XorStr( "48 8D 05 ? ? ? ? 48 89 05 ? ? ? ? 4C 8D 0D ? ? ? ? 0F B6 44 24 ? 4D 8D 43 ? 24 F9" ) ) );
			if ( !RefSite )
			{
				DEV_LOG( "[error] CNetworkMessages reference not found - feature disabled\n" );

				return nullptr;
			}

			const uintptr_t Vtable = ResolveRipRef( RefSite + 3 , RefSite + 7 );
			const uintptr_t Object = ResolveRipRef( RefSite + 10 , RefSite + 14 );

			uintptr_t ModuleBase = 0;
			uintptr_t ModuleEnd = 0;
			const bool bValid = GetModuleImageRange( NETWORKSYSTEM_DLL , ModuleBase , ModuleEnd )
				&& Object >= ModuleBase && Object < ModuleEnd
				&& *reinterpret_cast< const uintptr_t* >( Object ) == Vtable;

			if ( !bValid )
			{
				DEV_LOG( "[error] CNetworkMessages resolve failed - feature disabled\n" );

				return nullptr;
			}

			g_NetworkMessages = Object;
		}

		return reinterpret_cast< void* >( g_NetworkMessages );
	}

	// CNetworkGameClient 的全局指针在 engine2.dll,由 CCreateGameClientJob 分配后写入:
	//   cmp cs:g_pNetworkGameClient, 0        -> 48 83 3D disp32 00
	// 特征码从该函数入口开始(disp 在 +8,后继指令在 +13),解出的是「指针槽」地址。
	// 同样只解析一次;槽位是运行期可变的,每次调用再解引用。
	auto Pointers::NetworkGameClient() -> void*
	{
		static bool bResolveAttempted = false;

		if ( !bResolveAttempted )
		{
			bResolveAttempted = true;

			const auto RefSite = reinterpret_cast< uintptr_t >( FindPattern( ENGINE2_DLL , XorStr( "53 48 83 EC 20 48 83 3D ? ? ? ? 00 48 8B D9 8B 0D ? ? ? ?" ) ) );
			if ( !RefSite )
			{
				DEV_LOG( "[error] NetworkGameClient reference not found - feature disabled\n" );

				return nullptr;
			}

			const auto SlotAddress = ResolveRipRef( RefSite + 8 , RefSite + 13 );

			uintptr_t ModuleBase = 0;
			uintptr_t ModuleEnd = 0;
			if ( !GetModuleImageRange( ENGINE2_DLL , ModuleBase , ModuleEnd )
				|| SlotAddress < ModuleBase || SlotAddress >= ModuleEnd )
			{
				DEV_LOG( "[error] NetworkGameClient resolve failed - feature disabled\n" );

				return nullptr;
			}

			g_ppNetworkGameClient = reinterpret_cast< void** >( SlotAddress );
		}

		if ( !g_ppNetworkGameClient )
			return nullptr;

		void* pNetworkGameClient = *g_ppNetworkGameClient;
		if ( !pNetworkGameClient )
			return nullptr; // 未连接 / 已销毁

		// 虚表必须落在 engine2.dll 内:对象被释放后槽位可能还留着旧值
		uintptr_t ModuleBase = 0;
		uintptr_t ModuleEnd = 0;
		const auto Vtable = *reinterpret_cast< const uintptr_t* >( pNetworkGameClient );
		if ( !GetModuleImageRange( ENGINE2_DLL , ModuleBase , ModuleEnd )
			|| Vtable < ModuleBase || Vtable >= ModuleEnd )
			return nullptr;

		return pNetworkGameClient;
	}
}
