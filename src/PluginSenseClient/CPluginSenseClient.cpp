#include "CPluginSenseClient.hpp"

#include <unordered_set>

#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/Interface/IEngineToClient.hpp>
#include <CS2/SDK/Interface/IGameEvent.hpp>
#include <CS2/SDK/Update/CCSGOInput.hpp>
#include <CS2/SDK/Types/CEntityData.hpp>
#include <CS2/SDK/FunctionListSDK.hpp>

#include <PluginSenseClient/GUI/CPluginSenseMenu.hpp>
#include <PluginSenseClient/Features/CHelper/HelperTimeline.hpp>
#include <PluginSenseClient/CPluginSenseGUI.hpp>
#include <PluginSenseClient/Fonts/CFontManager.hpp>
#include <PluginSenseClient/Render/CRenderStackSystem.hpp>
#include <PluginSenseClient/Render/CRender.hpp>
#include <PluginSenseClient/Features/CNameChanger/CNameChanger.hpp>
#include <PluginSenseClient/Features/CChatSpammer/CChatSpammer.hpp>
#include <PluginSenseClient/Features/CKillSay/CKillSay.hpp>
#include <PluginSenseClient/Features/CFortniteDamage/CFortniteDamage.hpp>
#include <PluginSenseClient/Features/CVelocityDisplay/CVelocityDisplay.hpp>
#include <PluginSenseClient/Features/CFakeCooldown/CFakeCooldown.hpp>
#include <PluginSenseClient/Features/CHitLog/CHitLog.hpp>
#include <PluginSenseClient/Features/CWeather/CWeather.hpp>
#include <PluginSenseClient/Features/CWorldVisuals/CWorldVisuals.hpp>
#include <PluginSenseClient/Features/CMotionCamera/CMotionCamera.hpp>
#include <PluginSenseClient/Features/CLobbySpoof/CLobbySpoof.hpp>
#include <PluginSenseClient/Features/CWeaponModel/CWeaponModel.hpp>
#include <PluginSenseClient/Features/CHelper/CHelper.hpp>
#include <PluginSenseClient/Features/CHelper/CHelperRecorder.hpp>
#include <PluginSenseClient/Features/CBulletSparks/CBulletSparks.hpp>
#include <PluginSenseClient/Features/CAimLock/CAimLock.hpp>
#include <PluginSenseClient/Settings/MenuState.hpp>
#include <GameClient/CL_Players.hpp>
#include <PluginSenseClient/GUI/framework_w/framework/menu.hh>

static CPluginSenseClient g_CPluginSenseClient{};

auto CPluginSenseClient::OnInit() -> void
{
	GetNameChanger()->Init();
	GetFortniteDamage()->Init();
	GetVelocityDisplay()->Init();
		GetFakeCooldown()->Init();
		GetWeather()->Init();
			GetMotionCamera()->Init();
		GetLobbySpoof()->Init();
	GetWeaponModel()->Init();
	GetBulletSparks()->Init();
	GetHelperRecorder()->Load();
	{
		std::unordered_set<int> hiddenIds;
		for ( auto& [ mapName , lineups ] : GetHelperRecorder()->MutableMaps() )
			for ( auto& lu : lineups )
				if ( lu.builtin_id >= 0 && lu.hidden )
					hiddenIds.insert( lu.builtin_id );
		helper_timeline::StartLoad( hiddenIds );
	}
}

auto CPluginSenseClient::OnDestroy() -> void
{
	GetLobbySpoof()->Shutdown();
	GetWeaponModel()->Shutdown();
}

auto CPluginSenseClient::OnFrameStageNotify( int FrameStage ) -> void
{
	if ( FrameStage == 6 )
		{
			GetVelocityDisplay()->OnFrame();
		}

		if ( FrameStage == 7 )
		{
			GetWeather()->on_frame_stage_notify();
			GetWeaponModel()->OnFrame();
		}
}

auto CPluginSenseClient::OnFireEventClientSide( IGameEvent* pGameEvent ) -> void
{
	// KillSay on player_death
	if ( _strcmpi( pGameEvent->GetName(), XorStr( "player_death" ) ) == 0 ) {
		if ( auto* pLocalController = GetCL_Players()->GetLocalPlayerController(); pLocalController ) {
			if ( auto* pAttacker = pGameEvent->GetPlayerController( XorStr( "attacker" ) ); pAttacker ) {
				if ( pAttacker == pLocalController )
					GetKillSay()->OnKill();
			}
		}
	}

	// Hitlog + Fortnite damage indicator
	if ( _strcmpi( pGameEvent->GetName(), XorStr( "player_hurt" ) ) == 0 )
	{
		GetFortniteDamage()->OnPlayerHurt( pGameEvent );

		if ( menu_state::damageLogEnabled )
		{
			auto* pLocalController = GetCL_Players()->GetLocalPlayerController();
			if ( pLocalController )
			{
				auto* pAttacker = pGameEvent->GetPlayerController( XorStr( "attacker" ) );
				auto* pVictim = pGameEvent->GetPlayerController( XorStr( "userid" ) );
				if ( pAttacker && pVictim )
				{
					bool weAttacked = ( pAttacker == pLocalController );
					if ( ( menu_state::hitlogEnabled && weAttacked ) || ( menu_state::hitlogVictim && pVictim == pLocalController ) )
					{
						int64_t dmg = pGameEvent->GetInt64( XorStr( "dmg_health" ) );
						int64_t hitgroup = pGameEvent->GetInt64( XorStr( "hitgroup" ) );
						int64_t hp = pGameEvent->GetInt64( XorStr( "health" ) );

						if ( dmg > 0 )
						{
							const char* hitNames[] = { "body", "head", "chest", "stomach", "left arm", "right arm", "left leg", "right leg" };
							const char* hitName = ( hitgroup >= 1 && hitgroup <= 7 ) ? hitNames[hitgroup] : "body";

							auto getPlayerName = []( void* ctrl ) -> const char* {
								if ( !ctrl ) return XorStr( "unknown" );
								// 用 schema 运行时解析的 m_sSanitizedPlayerName,替代已过期的硬编码 0x6F4
								const char* s = static_cast<CCSPlayerController*>( ctrl )->m_sSanitizedPlayerName();
								return ( s && s[ 0 ] ) ? s : XorStr( "unknown" );
							};

							auto getTeamColor = []( void* ctrl ) -> const char* {
								if ( !ctrl ) return XorStr( "#c4c4c4" );
								uint8_t team = *reinterpret_cast<uint8_t*>( reinterpret_cast<uintptr_t>( ctrl ) + 0x3E7 );
								return team == 2 ? XorStr( "#ffd494" ) : ( team == 3 ? XorStr( "#85acfa" ) : XorStr( "#c4c4c4" ) );
							};

							auto getCompColor = []( void* ctrl ) -> const char* {
								if ( !ctrl ) return XorStr( "#c4c4c4" );
								int compColor = *reinterpret_cast<int*>( reinterpret_cast<uintptr_t>( ctrl ) + 0x0850 );
								switch ( compColor )
								{
								case 0: return XorStr( "#88CEF5" );
								case 1: return XorStr( "#009E80" );
								case 2: return XorStr( "#F1E441" );
								case 3: return XorStr( "#E6802A" );
								case 4: return XorStr( "#BD2C96" );
								default: return XorStr( "#c4c4c4" );
								}
							};


							char msg[512];
							void* targetCtrl = weAttacked ? pVictim : pAttacker;
							const char* targetName = getPlayerName( targetCtrl );
							const char* teamCol = getTeamColor( targetCtrl );
							const char* compCol = getCompColor( targetCtrl );

							if ( menu_state::hitlogType == 0 )
							{
								if ( weAttacked )
									snprintf( msg, sizeof( msg ), XorStr( "<font color=\"#FFFFFF\">Plugin</font><font color=\"#9DC41D\">Sense</font> <font color=\"#c4c4c4\">| Hit</font> <font color=\"%s\">%s</font><font color=\"#c4c4c4\"> in the %s for %lld damage (%lld health remaining)" ),
										teamCol, targetName, hitName, dmg, hp > 0 ? hp : 0 );
								else
									snprintf( msg, sizeof( msg ), XorStr( "<font color=\"#FFFFFF\">Plugin</font><font color=\"#9DC41D\">Sense</font> <font color=\"#c4c4c4\">| Harmed by</font> <font color=\"%s\">%s</font><font color=\"#c4c4c4\"> in the %s for %lld damage" ),
										teamCol, targetName, hitName, dmg );
							}
							else
							{
								if ( weAttacked )
									snprintf( msg, sizeof( msg ), XorStr( "[<font color=\"#FF0000\">Meme</font><font color=\"#FFFFFF\">Sense</font>] Hit <font color=\"%s\">\xE2\x97\x8F</font> <font color=\"%s\">%s</font><font color=\"#FFFFFF\"> in the %s for %lld damage (%lld health remaining)" ),
										compCol, teamCol, targetName, hitName, dmg, hp > 0 ? hp : 0 );
								else
									snprintf( msg, sizeof( msg ), XorStr( "[<font color=\"#FF0000\">Meme</font><font color=\"#FFFFFF\">Sense</font>] Harmed by <font color=\"%s\">\xE2\x97\x8F</font> <font color=\"%s\">%s</font><font color=\"#FFFFFF\"> in the %s for %lld damage" ),
										compCol, teamCol, targetName, hitName, dmg );
							}

							GetHitLog()->Send( msg );
						}
					}
				}
			}
		}
	}

	// Reset KillSay index on new round
	if ( _strcmpi( pGameEvent->GetName(), XorStr( "round_start" ) ) == 0 )
		GetKillSay()->ResetIndex();
}

auto CPluginSenseClient::OnRender() -> void
{
	// 手雷轨迹 PiP 预览已移入 CHelper::UpdateGrenadePreview(状态机派生,每帧 Tick 开头执行)

	GetNameChanger()->OnFrame();
	GetChatSpammer()->OnFrame();

	GetFontManager()->FirstInitFonts();

	// 游戏内 HUD:统一画背景层(CRender::GetHudDrawList),并在菜单之前绘制,
	// 保证菜单始终在顶层(同层最后绘制 = 最上层)。
	if ( SDK::Interfaces::EngineToClient()->IsInGame()
		&& GetCL_Players()->GetLocalPlayerController()
		&& GetCL_Players()->GetLocalPlayerPawn() )
	{
		GetRenderStackSystem()->OnRenderStack();
		GetFortniteDamage()->OnRender( CRender::GetHudDrawList() );
		int w, h;
		SDK::Interfaces::EngineToClient()->GetScreenSize( w, h );
		GetVelocityDisplay()->OnRender( CRender::GetHudDrawList(), w, h );
		GetMotionCamera()->on_render( CRender::GetHudDrawList(), w, h );
		GetHelper()->OnRender( CRender::GetHudDrawList(), w, h );
		GetAimLock()->OnRender( CRender::GetHudDrawList(), w, h );
	}

	// 菜单最后绘制,置于所有 HUD 之上
	if ( GetPluginSenseGUI()->IsVisible() )
		GetPluginSenseMenu()->OnRenderMenu();
	else
		GetPluginSenseMenu()->OnRenderWidgets();
}

auto CPluginSenseClient::OnClientOutput() -> void
{
}

auto CPluginSenseClient::OnCreateMove( CCSGOInput* pInput , CUserCmd* pUserCmd ) -> void
{
	GetWorldVisuals()->on_create_move( pInput );
	GetMotionCamera()->on_create_move( pInput );
	GetVelocityDisplay()->OnCreateMove( pUserCmd );
	GetHelper()->OnCreateMove( pInput , pUserCmd );
	GetAimLock()->OnCreateMove( pInput );
}

auto GetPluginSenseClient() -> CPluginSenseClient*
{
	return &g_CPluginSenseClient;
}
