#include "CServerLagger.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <vector>

#include <Common/MemoryEngine.hpp>
#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/Update/VMT_Index.hpp>
#include <PluginSenseClient/GUI/framework_w/includes.hh> // key_var_t(完整框架链,对齐 CAimLock.cpp)
#include <PluginSenseClient/Settings/MenuState.hpp>

namespace V = SDK::VMT_Index;

static CServerLagger g_CServerLagger{};

namespace
{
	// ============================================================ 档案
	// 两个档案只是「单条消息多大 / 一个数据报塞多少条」的取舍;
	// 每 tick 数据报上限统一 14(2.cpp 作者实测的安全线,超过会触发游戏侧溢出报错):
	//  · 模式 0:小包(1475 字节负载偏移),靠条数堆量
	//  · 模式 1:大包(16320 字节),条数少但单条超出引擎常规消息额度,慎用
	struct server_lagger_profile_t
	{
		std::uint32_t messages_per_datagram;
		int maximum_datagrams_per_tick;
		std::size_t packet_offsets_per_message;
	};

	constexpr server_lagger_profile_t kModeOneProfile = { 65, 14, 1475 };
	constexpr server_lagger_profile_t kModeTwoProfile = { 6, 14, 16320 };

	// clc_VoiceData = 22(netmessages.proto;svc_VoiceData 是 47,方向相反不要拿错)
	constexpr int kVoiceDataMessageId = 22;

	// xuid 每次随机(参考实现如此);函数内静态,避免在 DLL 静态初始化期构造
	std::uint64_t next_voice_xuid( )
	{
		static std::mt19937_64 generator{ std::random_device{}( ) };

		return generator( );
	}

	// ============================================================ bf_read 布局
	// 直接喂给引擎的位流读取器,布局必须与 CS2 内部的 bf_read 完全一致(0x28 字节)。
	struct bit_read_t
	{
		const void* data;         // 0x00
		std::int32_t data_bytes;  // 0x08
		std::int32_t data_bits;   // 0x0C
		std::int32_t current_bit; // 0x10
		std::uint32_t reserved;   // 0x14
		const char* debug_name;   // 0x18
		bool overflow;            // 0x20
		bool initialized;         // 0x21
		bool dword_safe;          // 0x22
		std::uint8_t tail[ 5 ];   // 0x23
	};

	static_assert( sizeof( bit_read_t ) == 0x28 );
	static_assert( offsetof( bit_read_t , overflow ) == 0x20 );

	// 单条语音消息的完整 protobuf 负载。数组按最大档案留足:
	//   10 字节字段头 + 16320 字节包偏移填充 + 0x11 标签 + 8 字节 xuid
	//   + 0x18 标签 + 最多 5 字节 tick varint
	struct voice_payload_t
	{
		std::array< std::uint8_t , 10 + 16320 + 1 + sizeof( std::uint64_t ) + 1 + 5 > bytes = { };
		std::size_t size = { };
	};

	// ============================================================ 虚调用
	// 参考实现走的是原始虚表下标;本项目用 vget 取槽位后强转成正确签名再调用。
	// 裸调虚表槽位前判空:解析到错误地址或槽位为空时返回默认值,由调用方当作失败处理,
	// 好过直接跳进野指针。
	template< typename Ret , typename... Args >
	auto invoke_vcall( void* instance , std::size_t index , Args... args ) -> Ret
	{
		using Fn = Ret( __fastcall* )( void* , Args... );

		if ( !instance )
			return Ret( );

		const Fn Function = vget< Fn >( instance , static_cast< unsigned int >( index ) );
		if ( !Function )
			return Ret( );

		return Function( instance , args... );
	}

	const server_lagger_profile_t& selected_server_lagger_profile( )
	{
		return menu_state::serverLaggerMode == 1 ? kModeTwoProfile : kModeOneProfile;
	}

	// 菜单里两个档案各有一条滑条,量程就是各自的上限;这里取当前档案对应那条
	int selected_server_lagger_amount( )
	{
		return menu_state::serverLaggerMode == 1 ? menu_state::serverLaggerAmountLarge : menu_state::serverLaggerAmountSmall;
	}

	void append_varint( std::uint32_t value , std::vector< std::uint8_t >& output )
	{
		do
		{
			std::uint8_t byte = static_cast< std::uint8_t >( value & 0x7Fu );
			value >>= 7u;
			if ( value )
				byte |= 0x80u;
			output.push_back( byte );
		} while ( value );
	}

	// 手工拼 clc_VoiceData(CCLCMsg_VoiceData):
	//   0A <len>        data    —— 内层 CMsgVoiceData(audio_payload_bytes 只用于把长度写大)
	//     08 02         字段 1(varint)= 2
	//     12 00         字段 2(len)= 空
	//     42 <len>      字段 8(len)= 包偏移表(packet_offsets_per_message 字节的填充区,内容留 0)
	//   <填充区之后> 11 <8 字节 xuid>   字段 2 fixed64
	//                 18 <varint tick> 字段 3 varint
	voice_payload_t make_voice_payload( const server_lagger_profile_t& profile , std::uint64_t xuid , std::uint32_t tick )
	{
		const std::size_t audio_payload_bytes = 2 + 2 + 3 + profile.packet_offsets_per_message;
		const std::array< std::uint8_t , 10 > prefix = {
			0x0A,
			static_cast< std::uint8_t >( ( audio_payload_bytes & 0x7F ) | 0x80 ),
			static_cast< std::uint8_t >( audio_payload_bytes >> 7u ),
			0x08,
			0x02,
			0x12,
			0x00,
			0x42,
			static_cast< std::uint8_t >( ( profile.packet_offsets_per_message & 0x7F ) | 0x80 ),
			static_cast< std::uint8_t >( profile.packet_offsets_per_message >> 7u ),
		};

		voice_payload_t payload;
		std::copy( prefix.begin( ) , prefix.end( ) , payload.bytes.begin( ) );

		std::size_t offset = prefix.size( ) + profile.packet_offsets_per_message;
		payload.bytes[ offset++ ] = 0x11;
		for ( std::size_t byte = 0; byte < sizeof( xuid ); ++byte )
			payload.bytes[ offset++ ] = static_cast< std::uint8_t >( xuid >> ( byte * 8u ) );

		payload.bytes[ offset++ ] = 0x18;
		do
		{
			std::uint8_t encoded = static_cast< std::uint8_t >( tick & 0x7Fu );
			tick >>= 7u;
			if ( tick )
				encoded |= 0x80u;
			payload.bytes[ offset++ ] = encoded;
		} while ( tick );

		payload.size = offset;
		return payload;
	}

	void destroy_message( void* message )
	{
		if ( message )
			invoke_vcall< void >( message , V::CNetMessagePB::Destroy , 1u );
	}

	// 从网络消息注册表里取 clc_VoiceData 的序列化器,分配一条空消息,
	// 再把 framed 负载反序列化进去;失败返回 nullptr(已分配的会被就地释放)。
	// 注意 UnSerializeMessage 自己会先从位流里读 varint 长度,所以 framed 必须带长度前缀。
	void* make_voice_message( const voice_payload_t& payload )
	{
		void* messages = SDK::Pointers::NetworkMessages();
		if ( !messages )
			return nullptr;

		void* record = invoke_vcall< void* >( messages , V::CNetworkMessages::GetNetMessageRecord , kVoiceDataMessageId );
		if ( !record )
			return nullptr;

		// GetNetMessageInfo 返回的是 record 内嵌的 NetMessageInfo_t(record + 0x10),
		// 于是 info + 0x08 正好是 CNetworkSerializerPB::protobuffBinding(record + 0x18):
		// 真正负责分配消息的对象。
		auto* info = invoke_vcall< std::uint8_t* >( messages , V::CNetworkMessages::GetNetMessageInfo , record );
		void* binding = info ? *reinterpret_cast< void** >( info + 0x08 ) : nullptr;
		if ( !binding )
			return nullptr;

		void* message = invoke_vcall< void* >( binding , V::CNetworkSerializerPB::AllocateMessage );
		if ( !message )
			return nullptr;

		// 网络消息自带 varint 长度前缀;尾部多留 4 字节给位流读取器的按 dword 取值
		std::vector< std::uint8_t > framed;
		framed.reserve( payload.size + 6 );
		append_varint( static_cast< std::uint32_t >( payload.size ) , framed );
		framed.insert( framed.end( ) , payload.bytes.begin( ) , payload.bytes.begin( ) + payload.size );
		const std::size_t logical_size = framed.size( );
		framed.resize( logical_size + 4 );

		bit_read_t reader = {
			framed.data( ),
			static_cast< std::int32_t >( logical_size ),
			static_cast< std::int32_t >( logical_size * 8 ),
			0,
			0,
			"Server Lagger",
			false,
			true,
			true,
			{ },
		};

		if ( !invoke_vcall< bool >( messages , V::CNetworkMessages::UnSerializeMessage , &reader , message ) || reader.overflow )
		{
			destroy_message( message );
			return nullptr;
		}

		return message;
	}

	// prototype 是模板消息:每条都克隆一份再投递,投递失败立刻停手(通道满了)
	void send_voice_payload( void* channel , const voice_payload_t& payload , const server_lagger_profile_t& profile , std::uint32_t datagrams )
	{
		void* prototype = make_voice_message( payload );
		if ( !prototype )
			return;

		bool transport_available = true;
		for ( std::uint32_t datagram = 0; datagram < datagrams && transport_available; ++datagram )
		{
			std::uint32_t batch_sent = 0;
			for ( ; batch_sent < profile.messages_per_datagram; ++batch_sent )
			{
				void* message = invoke_vcall< void* >( prototype , V::CNetMessagePB::Clone );
				if ( !message )
				{
					transport_available = false;
					break;
				}

				const bool accepted = invoke_vcall< bool >( channel , V::CNetChan::SendNetMessage , message , static_cast< std::int8_t >( -1 ) );
				destroy_message( message );
				if ( !accepted )
				{
					transport_available = false;
					break;
				}
			}

			// 一批塞满就 Transmit 一次,把缓冲交给传输层;不调就要等引擎的常规发送
			if ( batch_sent )
				invoke_vcall< std::int32_t >( channel , V::CNetChan::Transmit , "Server Lagger" , static_cast< void* >( nullptr ) );
		}

		destroy_message( prototype );
	}
}

void CServerLagger::OnFrame( )
{
	// 勾选框是总开关,热键只是触发条件(对齐 aimlock/helper 的键语义,键不回写开关):
	// 没绑键视为恒激活;绑了键则 Hold/Toggle/Always(见 palette)决定激活区间。
	const int toggle_key = server_lagger::g_toggle_key.key;
	const bool key_active = ( toggle_key <= 0 || toggle_key > 255 ) || server_lagger::g_toggle_key.active();

	if ( !menu_state::serverLagger || !key_active )
	{
		m_Runtime = { };
		return;
	}

	void* network_client = SDK::Pointers::NetworkGameClient( );
	if ( !network_client )
	{
		m_Runtime = { };
		return;
	}

	const int current_tick = invoke_vcall< int >( network_client , V::CNetworkGameClient::GetClientTickCount );
	if ( m_Runtime.network_client == network_client && m_Runtime.tick == current_tick )
		return;
	m_Runtime = { network_client , current_tick };

	void* channel = invoke_vcall< void* >( network_client , V::CNetworkGameClient::GetNetChannel , 0 );
	if ( !channel || !invoke_vcall< bool >( channel , V::CNetChan::CanPacket ) )
		return;

	const server_lagger_profile_t& profile = selected_server_lagger_profile( );
	// 滑条量程已经和档案一致,这里的 clamp 只是兜底(手改 JSON 之类)
	const std::uint32_t amount = static_cast< std::uint32_t >( std::clamp( selected_server_lagger_amount( ) , 1 , profile.maximum_datagrams_per_tick ) );
	const voice_payload_t payload = make_voice_payload( profile , next_voice_xuid( ) , static_cast< std::uint32_t >( current_tick ) );
	send_voice_payload( channel , payload , profile , amount );
}

auto GetServerLagger() -> CServerLagger*
{
	return &g_CServerLagger;
}
