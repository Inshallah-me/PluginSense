#include "Hook_CreateMove.hpp"

#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/Interface/IEngineToClient.hpp>
#include <CS2/SDK/Update/CCSGOInput.hpp>

#include <PluginSenseClient/CPluginSenseClient.hpp>
#include <PluginSenseClient/CPluginSenseGUI.hpp>

#include <GameClient/CL_Players.hpp>
#include <GameClient/CL_Bypass.hpp>

namespace
{
	constexpr uint64_t MENU_CAPTURED_BUTTONS =
		IN_ATTACK | IN_ATTACK2 |
		IN_JUMP | IN_DUCK |
		IN_FORWARD | IN_BACK | IN_MOVELEFT | IN_MOVERIGHT |
		IN_USE | IN_RELOAD | IN_SPEED |
		IN_TURNLEFT | IN_TURNRIGHT;

	auto ClearMenuCapturedButtons( CUserCmd* pUserCmd ) -> void
	{
		pUserCmd->button_states.buttonstate1 &= ~MENU_CAPTURED_BUTTONS;
		pUserCmd->button_states.buttonstate2 &= ~MENU_CAPTURED_BUTTONS;
		pUserCmd->button_states.buttonstate3 &= ~MENU_CAPTURED_BUTTONS;

		if ( auto* pBase = pUserCmd->cmd.mutable_base(); pBase )
		{
			pBase->set_forwardmove( 0.f );
			pBase->set_leftmove( 0.f );
			pBase->clear_subtick_moves();

			auto* pButtons = pBase->mutable_buttons_pb();
			pButtons->set_buttonstate1( pUserCmd->button_states.buttonstate1 );
			pButtons->set_buttonstate2( pUserCmd->button_states.buttonstate2 );
			pButtons->set_buttonstate3( pUserCmd->button_states.buttonstate3 );
		}
	}
}

auto Hook_CreateMove( CCSGOInput* pCCSGOInput , uint32_t split_screen_index , char a3 ) -> bool
{
	auto Result = false;

	if ( auto* pLocalPlayerController = GetCL_Players()->GetLocalPlayerController(); pLocalPlayerController )
	{
		Result = CreateMove_o( pCCSGOInput , split_screen_index , a3 );
		const auto pUserCmd = pCCSGOInput->GetUserCmd( pLocalPlayerController );

		if ( !pUserCmd )
			return Result;

		if ( GetPluginSenseGUI()->IsVisible() )
		{
			ClearMenuCapturedButtons( pUserCmd );
			return Result;
		}

		if ( SDK::Interfaces::EngineToClient()->IsInGame() && GetCL_Players()->GetLocalPlayerPawn() )
		{
			GetCL_Bypass()->PreClientCreateMove( pUserCmd );
			GetPluginSenseClient()->OnCreateMove( pCCSGOInput , pUserCmd );
			GetCL_Bypass()->PostClientCreateMove( pCCSGOInput , pUserCmd );
		}
	}

	return Result;
}

auto Hook_MessageLite_SerializePartialToArray( google::protobuf::Message* pMsg , void* out_buffer , int size ) -> bool
{
#if DISABLE_PROTOBUF == 0
	const google::protobuf::Descriptor* descriptor = pMsg->GetDescriptor();
	const std::string message_name = descriptor->name();

	if ( message_name == XorStr( "CBaseUserCmdPB" ) )
	{
		GetCL_Bypass()->OnCBaseUserCmdPB( reinterpret_cast<CBaseUserCmdPB*>( pMsg ) );
	}
#endif

	return MessageLite_SerializePartialToArray_o( pMsg , out_buffer , size );
}
