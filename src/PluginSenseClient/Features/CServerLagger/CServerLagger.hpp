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
// 每个新 tick 最多发 serverLaggerAmountSmall / serverLaggerAmountLarge
// (按当前档案二选一)个数据报,每个数据报内打包 messages_per_datagram 条消息。
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
};

auto GetServerLagger() -> CServerLagger*;
