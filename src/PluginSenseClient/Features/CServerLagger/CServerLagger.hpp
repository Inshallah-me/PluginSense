#pragma once

#include <Common/Common.hpp>

namespace framework
{
	struct key_var_t;
}

namespace server_lagger
{
	// 触发热键:默认不绑(= 恒激活);绑定时 Hold/Toggle/Always 由 palette 里的模式决定。
	// 键只是触发条件,不回写开关 —— 总开关始终是勾选框。
	extern framework::key_var_t g_toggle_key;
}

// Server Lagger:按客户端 tick 向服务器批量发送 clc_VoiceData 语音消息。
// 发送节奏为 burst/rest 状态机(常量与说明见 CServerLagger.cpp):连发、饱和、歇息轮转,
// 避免满速连发把发送队列灌满后反而降低送达率。
class CServerLagger final
{
public:
	auto OnFrame() -> void;

private:
	// 上一次发包时的网络客户端与 tick:同一个 (客户端, tick) 只发一次
	struct runtime_t
	{
		void* network_client = nullptr;
		int tick = -1;
	};

	runtime_t m_Runtime{};
	// 状态机:m_CycleTick 在连发阶段记录已投出的拍数,到 kBurstTicks 转入歇息,
	// 歇满 kRestTicks 拍归零;m_BlockedTicks 记连续被 CanPacket 拦下的拍数(饱和判定)
	int m_CycleTick = 0;
	int m_BlockedTicks = 0;
};

auto GetServerLagger() -> CServerLagger*;
