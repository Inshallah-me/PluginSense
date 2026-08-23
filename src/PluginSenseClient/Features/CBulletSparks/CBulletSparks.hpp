#pragma once

#include <Common/Common.hpp>

class IGameEvent;

class CBulletSparks final
{
public:
	auto Init() -> void;
	auto OnBulletImpact( IGameEvent* pGameEvent ) -> void;
	auto RegisterListener( void* mgr ) -> bool;

private:
	// IGameEventManager2 监听器(bullet_impact 不走 FireEventClientSide,需 AddListener 注册)
	struct Listener
	{
		void** vtable;
		int debug_id;
	};

	static void* __fastcall FireGameEvent( void* self , void* event );
	static int __fastcall GetEventDebugID( void* self );

private:
	static void* s_Vtable[ 3 ];
	Listener m_Listener{ s_Vtable , 1 };
	bool m_bListenerRegistered = false;
	bool m_bLoaded = false;
};

auto GetBulletSparks() -> CBulletSparks*;
