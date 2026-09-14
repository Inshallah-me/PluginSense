#pragma once

namespace SDK::VMT_Index
{
	enum CSchemaSystem : uint32_t
	{
		GlobalTypeScope = 11 ,
		SchemaClassInfo = 46 ,
	};
	enum IVEngineClient2 : uint32_t
	{
		GetScreenSize = 61 ,
	};
	enum CGameSceneNode : uint32_t
	{
		PostDataUpdate = 25 ,
	};

	// ---- Server Lagger 用到的虚表下标(已按目标构建逐槽反编译核对) -------------------
	//
	// CNetworkMessages(networksystem.dll 静态单例,见 SDK::Pointers::NetworkMessages):
	// GetNetMessageRecord 返回的就是 CNetworkSerializerPB*;它的 +0x10 是内嵌的 NetMessageInfo_t,
	// 所以 GetNetMessageInfo(record) 实际返回 record + 0x10,而 info + 0x08 正好落在
	// CNetworkSerializerPB::protobuffBinding(record + 0x18)上 —— 那才是真正分配消息的对象。
	enum CNetworkMessages : uint32_t
	{
		UnSerializeMessage = 4 ,    // bool( bf_read* reader , CNetMessagePB* message ) —— 先读 varint 长度再反序列化
		GetNetMessageInfo = 12 ,    // NetMessageInfo_t*( record ) —— 内部是 record->vtbl[2]()
		GetNetMessageRecord = 30 ,  // CNetworkSerializerPB*( int messageID ) —— 哈希表按 messageID 查
	};

	enum CNetworkSerializerPB : uint32_t
	{
		AllocateMessage = 6 ,       // CNetMessagePB*( ) —— 转调 protobuffBinding->vtbl[6]()
	};

	// CNetMessagePB<id, proto, ...>(具体实例见 CServerLagger.cpp 的 clc_VoiceData)
	enum CNetMessagePB : uint32_t
	{
		Destroy = 0 ,               // 删除析构 —— 传 1 就地 delete
		Clone = 4 ,                 // CNetMessagePB*( ) —— 深拷贝一条消息
	};

	// CNetworkGameClient(engine2.dll 全局指针,见 SDK::Pointers::NetworkGameClient)
	enum CNetworkGameClient : uint32_t
	{
		GetClientTickCount = 5 ,    // int( ) —— 返回 this+0x378(客户端 tick,非 serverTickCount)
		GetNetChannel = 41 ,        // CNetChan*( int slot ) —— slot -1 归一为 0
	};

	// CNetChan(networksystem.dll)
	enum CNetChan : uint32_t
	{
		SendNetMessage = 39 ,       // bool( CNetMessagePB* message , char reliableFlag ) —— -1 = 用消息自带默认值
		Transmit = 41 ,             // int( const char* debugName , bf_write* extra ) —— extra 可为 nullptr
		CanPacket = 47 ,            // bool( ) —— 抑制发送 / SNP 队列堆积时返回 false
	};
}
