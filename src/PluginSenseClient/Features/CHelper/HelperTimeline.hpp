#pragma once

#include <Common/Common.hpp>

#include <atomic>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <CS2/SDK/Math/Vector3.hpp>
#include <CS2/SDK/Math/QAngle.hpp>

// ============================================================================
// ============================================================================
// 点位时间线库:逐 usercmd 输入时间线(按钮/视角/位置)。
// 数据编译自 HelperTimelineData.hpp(生成器 tools/gen_timeline_data.py),
// 构建内存库后提供"当前地图的点位列表"给 Helper 的收集/渲染/回放。
// ============================================================================
namespace helper_timeline
{
	// 一条 usercmd 的输入帧(按钮位按 CS2 IN_* 枚举解码成布尔)
	struct Frame
	{
		QAngle angles{};
		Vector3 position{}; // 录制时该帧的玩家位置(漂移检测用)
		bool in_attack = false;
		bool in_attack2 = false;
		bool in_jump = false;
		bool in_duck = false;
		bool in_forward = false;
		bool in_back = false;
		bool in_moveleft = false;
		bool in_moveright = false;
		bool in_speed = false;
		bool in_use = false;
	};

	// 一条点位:站位 + 视角 + 投掷帧序列
	struct Point
	{
		int id = 0;
		std::string name;
		std::string weapon;    // "weapon_smokegrenade" 等
		std::uint8_t kind = 0; // resources::nades::kind
		Vector3 position{};    // 站位(sp)
		QAngle angles{};       // 站位视角(sv)
		std::vector<Frame> frames;
		bool hidden = false;   // 用户隐藏(存于 helper_lineups.dat 的覆盖条目)
	};

	// 构建嵌入点位库(hiddenIds = 要标记隐藏的点位 id 集合)
	auto StartLoad( const std::unordered_set<int>& hiddenIds = {} ) -> void;
	auto Ready() -> bool;

	// 库就绪后同步一次隐藏状态(点位库先于自录表加载时用)
	auto ApplyHiddenOverrides( const std::unordered_set<int>& hiddenIds ) -> void;

	// 当前地图的点位(未就绪/无数据返回 nullptr;指针在 Ready 后稳定)
	auto GetMapPoints( const std::string& mapName ) -> const std::vector<Point>*;

	// 按点位库 id 查点位(跨地图,供隐藏覆盖的写入/恢复),无则 nullptr
	auto FindPointById( int id ) -> Point*;
}
