#include "CFortniteDamage.hpp"
#include <vector>
#include <algorithm>
#include <cmath>
#include <chrono>

#include <Common/Common.hpp>
#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/Math/Math.hpp>
#include <CS2/SDK/Interface/IEngineToClient.hpp>
#include <CS2/SDK/Interface/IGameEvent.hpp>
#include <CS2/SDK/Interface/CGameEntitySystem.hpp>
#include <CS2/SDK/Types/CEntityData.hpp>
#include <CS2/SDK/Math/Vector3.hpp>
#include <ImGui/imgui.h>
#include <PluginSenseClient/Assets/fortnite_digits.hpp>
#include <GameClient/CL_Players.hpp>

namespace menu_state
{
	extern bool damageIndicator;
	extern float damageScale;
	extern float damageTime;
	extern ImVec4 damageBody;
	extern ImVec4 damageHead;
}

struct DamageIndicator
{
	int handle = 0;
	int damage = 0;
	Vector3 position = {};
	float spawnTime = 0.f;
	bool headshot = false;
};

static CFortniteDamage g_CFortniteDamage{};
static std::vector<DamageIndicator> s_indicators;

static float QuartIn( float t ) noexcept
{
	t = std::clamp( t, 0.f, 1.f );
	return t * t * t * t;
}

static float ElasticOut( float t ) noexcept
{
	t = std::clamp( t, 0.f, 1.f );
	if ( t <= 0.f || t >= 1.f )
		return t;
	constexpr float c4 = ( 2.f * 3.1415926535f ) / 3.f;
	return std::pow( 2.f, -10.f * t ) * std::sin( ( t * 10.f - 0.75f ) * c4 ) + 1.f;
}

static ImU32 ColorWithAlpha( const ImVec4& color, float alphaScale ) noexcept
{
	ImVec4 c = color;
	c.w = std::clamp( c.w * alphaScale, 0.f, 1.f );
	return ImGui::ColorConvertFloat4ToU32( c );
}

void CFortniteDamage::Init()
{
	// fortnite_digits is already initialized by CPluginSenseMenu::OnGuiInitialized
}

void CFortniteDamage::OnPlayerHurt( IGameEvent* event )
{
	if ( !menu_state::damageIndicator )
		return;

	DEV_LOG( "[FortniteDamage] OnPlayerHurt\n" );

	// verify local player is attacker
	auto* pLocalController = GetCL_Players()->GetLocalPlayerController();
	if ( !pLocalController )
		return;

	auto* pAttacker = event->GetPlayerController( XorStr( "attacker" ) );
	if ( pAttacker != pLocalController )
		return;

	auto* pVictimController = event->GetPlayerController( XorStr( "userid" ) );
	if ( !pVictimController )
		return;

	auto& victimPawn = pVictimController->m_hPawn();
	if ( !victimPawn.IsValid() )
		return;

	auto* pPawn = victimPawn.Get<C_CSPlayerPawn>();
	if ( !pPawn )
		return;

	const int damage = static_cast<int>( event->GetInt64( XorStr( "dmg_health" ) ) );
	const int hitgroup = static_cast<int>( event->GetInt64( XorStr( "hitgroup" ) ) );

	if ( damage <= 0 )
		return;

	DEV_LOG( "[FortniteDamage] dmg=%d hitgroup=%d\n" , damage , hitgroup );

	const Vector3 pos = pPawn->m_vOldOrigin() + reinterpret_cast<const Vector3&>( pPawn->m_vecViewOffset() );
	const float now = static_cast<float>( ImGui::GetTime() );
	const bool headshot = ( hitgroup == 1 );
	const int victimHandle = victimPawn.GetEntryIndex();

	DEV_LOG( "[FortniteDamage] pos=(%.1f,%.1f,%.1f) headshot=%d\n" , pos.m_x , pos.m_y , pos.m_z , headshot );

	// accumulate for same handle within 0.3s
	for ( auto& ind : s_indicators ) {
		if ( ind.handle == victimHandle && ( now - ind.spawnTime ) < 0.3f ) {
			ind.damage += damage;
			ind.position = pos;
			ind.spawnTime = now;
			ind.headshot = ind.headshot || headshot;
			return;
		}
	}

	s_indicators.push_back( { victimHandle, damage, pos, now, headshot } );
}

void CFortniteDamage::OnRender( ImDrawList* drawList )
{
	if ( !menu_state::damageIndicator || !drawList || !fortnite_digits::Ready() )
		return;

	const float now = static_cast<float>( ImGui::GetTime() );
	const float timeout = std::clamp( menu_state::damageTime, 1.f, 10.f );

	for ( auto it = s_indicators.begin(); it != s_indicators.end(); ) {
		const float age = now - it->spawnTime;
		if ( age >= timeout ) {
			it = s_indicators.erase( it );
			continue;
		}

		ImVec2 screen;
		if ( Math::WorldToScreen( it->position, screen ) ) {
			const float progress = std::clamp( age / timeout, 0.f, 1.f );
			const float fade = QuartIn( progress );
			const float alpha = 1.f - fade;
			const float baseScale = std::clamp( menu_state::damageScale, 0.25f, 2.f ) * 0.5f;
			const std::string text = std::to_string( it->damage );
			const float digitAdvance = static_cast<float>( fortnite_digits::kSourceWidth ) * baseScale;
			const float totalWidth = digitAdvance * static_cast<float>( text.size() );
			const float startX = screen.x - totalWidth * 0.5f + digitAdvance * 0.5f;
			const float y = screen.y - fade * 256.f;

			const ImVec4 color = it->headshot ? menu_state::damageHead : menu_state::damageBody;

			for ( size_t i = 0; i < text.size(); ++i ) {
				const int digit = text[ i ] - '0';
				int texW = 0, texH = 0;
				ImTextureID tex = fortnite_digits::Get( digit, texW, texH );
				if ( !tex || texW <= 0 || texH <= 0 )
					continue;

				const float digitDuration = 1.f + static_cast<float>( i + 1 ) * 0.25f;
				const float elastic = ElasticOut( age / digitDuration );
				const float animScale = (std::max)( 0.01f, baseScale * ( elastic - fade ) );
				const ImVec2 size( static_cast<float>( texW ) * animScale, static_cast<float>( texH ) * animScale );
				const ImVec2 pos( startX + static_cast<float>( i ) * digitAdvance, y );
				const ImVec2 imageMin( pos.x - size.x * 0.5f, pos.y - size.y * 0.5f );
				const ImVec2 imageMax( imageMin.x + size.x, imageMin.y + size.y );

				drawList->AddImage( tex, imageMin, imageMax, ImVec2( 0.f, 0.f ), ImVec2( 1.f, 1.f ), ColorWithAlpha( color, alpha ) );
			}
		}
		++it;
	}
}

void CFortniteDamage::OnLevelShutdown()
{
	s_indicators.clear();
}

auto GetFortniteDamage() -> CFortniteDamage*
{
	return &g_CFortniteDamage;
}
