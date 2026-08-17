#pragma once

#include <Common/Common.hpp>

#include <CS2/SDK/Math/Vector3.hpp>

class IGameEvent;
class CCSGOInput;
class CUserCmd;

class IPluginSenseClient
{
public:
	virtual void OnFrameStageNotify( int FrameStage ) = 0;
	virtual void OnFireEventClientSide( IGameEvent* pGameEvent ) = 0;
	virtual void OnRender() = 0;
	virtual void OnClientOutput() = 0;
	virtual void OnCreateMove( CCSGOInput* pInput , CUserCmd* pUserCmd ) = 0;
};

class CPluginSenseClient final : public IPluginSenseClient
{
public:
	auto OnInit() -> void;
	auto OnDestroy() -> void;

public:
	virtual void OnFrameStageNotify( int FrameStage ) override;
	virtual void OnFireEventClientSide( IGameEvent* pGameEvent ) override;
	virtual void OnRender() override;
	virtual void OnClientOutput() override;
	virtual void OnCreateMove( CCSGOInput* pInput , CUserCmd* pUserCmd ) override;
};

auto GetPluginSenseClient() -> CPluginSenseClient*;
