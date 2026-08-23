#include "CDamageEffect.hpp"

#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/Interface/IGameEvent.hpp>
#include <CS2/SDK/Types/CEntityData.hpp>
#include <CS2/SDK/FunctionListSDK.hpp>

#include <GameClient/CL_Players.hpp>
#include <PluginSenseClient/Settings/MenuState.hpp>

static CDamageEffect g_DamageEffect{};

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

auto CDamageEffect::OnInit() -> void
{
}

auto CDamageEffect::OnPlayerHurt( IGameEvent* pGameEvent ) -> void
{
	if ( !menu_state::deathEffect )
		return;

	// 只处理本地击杀(本地攻击 + 非本地受害者 + 死亡)
	auto* pLocal = GetCL_Players()->GetLocalPlayerController();
	if ( !pLocal )
		return;

	auto* pAttacker = pGameEvent->GetPlayerController( XorStr( "attacker" ) );
	auto* pVictim = pGameEvent->GetPlayerController( XorStr( "userid" ) );
	if ( pAttacker != pLocal || !pVictim || pVictim == pLocal )
		return;

	if ( pGameEvent->GetInt64( XorStr( "health" ) ) > 0 )
		return;

	// 从受害者 controller 获取 pawn
	auto& hPawn = pVictim->m_hPawn();
	if ( !hPawn.IsValid() )
		return;

	auto* pPawn = hPawn.Get<C_CSPlayerPawn>();
	if ( !pPawn )
		return;

	PlayDeathEffect( pPawn );
}

void CDamageEffect::PlayDeathEffect( C_CSPlayerPawn* pPawn )
{
	auto* pParticleManager = SDK::Pointers::ParticleManager();
	if ( !pParticleManager )
		return;

	constexpr const char* kFadePath = "particles/embedded/fade.vpcf";

	if ( !m_bLoaded )
	{
		BufferString buffer{};
		buffer.m_unknown2 = 0xc00000c8;
		InitParticlePathBuffer( &buffer, kFadePath );
		buffer.m_unknown4 = 'fcpv';
		ResourceSystemLoad( SDK::Interfaces::ResourceSystem(), &buffer, "" );
		m_bLoaded = true;
	}

	int effectIndex = -1;
	ParticleCreateEffect( pParticleManager, &effectIndex, kFadePath, 8, 0, 0, 0, 0 );
	if ( effectIndex == -1 )
		return;

	// control point 2 = 颜色
	const Vector3 color(
		menu_state::deathEffectColor.x * 255.f,
		menu_state::deathEffectColor.y * 255.f,
		menu_state::deathEffectColor.z * 255.f );
	ParticleSetControlPoint( pParticleManager, effectIndex, 2, const_cast<Vector3*>( &color ), 0 );

	// 绑定粒子到受害者实体(跟随尸体)
	struct DefaultPos
	{
		std::intptr_t xy{ 0x7f7fffff7f7fffffll };
		int z{ 0x7f7fffff };
	} defaultPos;

	ParticleSetEntityBinding( pParticleManager, effectIndex, 0, pPawn, 1, nullptr, &defaultPos, 1, 0 );
	ParticleSetEntityBinding( pParticleManager, effectIndex, 1, pPawn, 1, nullptr, &defaultPos, 1, 0 );
}

auto GetDamageEffect() -> CDamageEffect*
{
	return &g_DamageEffect;
}
