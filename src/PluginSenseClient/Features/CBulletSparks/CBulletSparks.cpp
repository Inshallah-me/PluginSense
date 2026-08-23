#include "CBulletSparks.hpp"

#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/Interface/IGameEvent.hpp>
#include <CS2/SDK/FunctionListSDK.hpp>

#include <GameClient/CL_Players.hpp>
#include <PluginSenseClient/Settings/MenuState.hpp>

static CBulletSparks g_BulletSparks{};

namespace
{
	struct BufferString
	{
		std::uint32_t m_unknown1{};
		std::uint32_t m_unknown2{ 0xc00000c8 };

		union
		{
			std::uintptr_t m_str_ptr;
			std::uint8_t data[ 0xc8 ];
		};

		std::uintptr_t m_unknown3{};
		std::uintptr_t m_unknown4{};
	};
}

void* CBulletSparks::s_Vtable[ 3 ] = { nullptr , &CBulletSparks::FireGameEvent , &CBulletSparks::GetEventDebugID };

void* __fastcall CBulletSparks::FireGameEvent( void* self , void* event )
{
	GetBulletSparks()->OnBulletImpact( reinterpret_cast<IGameEvent*>( event ) );
	return nullptr;
}

int __fastcall CBulletSparks::GetEventDebugID( void* self )
{
	return 0;
}

auto CBulletSparks::Init() -> void
{
	// 监听器在 FireEventClientSide hook 首次触发时注册(需要 IGameEventManager2 指针)
}

auto CBulletSparks::RegisterListener( void* mgr ) -> bool
{
	if ( !mgr )
		return false;

	if ( m_bListenerRegistered )
		return true;

	// IGameEventManager2 vtable[3] = AddListener(listener, name, serverside)
	using AddListenerFn = bool( __fastcall* )( void* , void* , const char* , bool );
	const auto add = reinterpret_cast<AddListenerFn>( ( *reinterpret_cast<void***>( mgr ) )[ 3 ] );
	if ( add( mgr , &m_Listener , "bullet_impact" , false ) )
	{
		m_bListenerRegistered = true;
		return true;
	}

	return false;
}

auto CBulletSparks::OnBulletImpact( IGameEvent* pGameEvent ) -> void
{
	if ( !menu_state::bulletSparks )
		return;

	// 只处理本地玩家的弹着点
	if ( auto* pShooter = pGameEvent->GetPlayerController( XorStr( "userid" ) ); pShooter )
	{
		if ( pShooter != GetCL_Players()->GetLocalPlayerController() )
			return;
	}

	const float x = pGameEvent->GetFloat( XorStr( "x" ) );
	const float y = pGameEvent->GetFloat( XorStr( "y" ) );
	const float z = pGameEvent->GetFloat( XorStr( "z" ) );

	auto* pParticleManager = SDK::Pointers::ParticleManager();
	if ( !pParticleManager )
		return;

	constexpr const char* kSparkPath = "particles/embedded/sparks.vpcf";

	if ( !m_bLoaded )
	{
		BufferString buffer{};
		buffer.m_unknown2 = 0xc00000c8;
		InitParticlePathBuffer( &buffer, kSparkPath );
		buffer.m_unknown4 = 'fcpv';
		ResourceSystemLoad( SDK::Interfaces::ResourceSystem(), &buffer, "" );
		m_bLoaded = true;
	}

	int effectIndex = -1;
	ParticleCreateEffect( pParticleManager, &effectIndex, kSparkPath, 8, 0, 0, 0, 0 );
	if ( effectIndex == -1 )
		return;

	const Vector3 position( x, y, z );
	ParticleSetControlPoint( pParticleManager, effectIndex, 0, const_cast<Vector3*>( &position ), 0 );

	const Vector3 color(
		menu_state::sparksColor.x * 255.f,
		menu_state::sparksColor.y * 255.f,
		menu_state::sparksColor.z * 255.f );
	ParticleSetControlPoint( pParticleManager, effectIndex, 1, const_cast<Vector3*>( &color ), 0 );
}

auto GetBulletSparks() -> CBulletSparks*
{
	return &g_BulletSparks;
}
