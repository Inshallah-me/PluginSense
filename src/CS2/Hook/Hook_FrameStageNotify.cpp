#include "Hook_FrameStageNotify.hpp"

#include <PluginSenseClient/CPluginSenseClient.hpp>

auto Hook_FrameStageNotify( CSource2Client* pCSource2Client , int FrameStage ) -> void
{
	GetPluginSenseClient()->OnFrameStageNotify( FrameStage );

	return FrameStageNotify_o( pCSource2Client , FrameStage );
}
