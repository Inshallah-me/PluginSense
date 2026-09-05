#pragma once

#include <cstdint>

// ============================================================================
// 道具类型枚举与动作位(全 Helper 共用)。
// (原 nade_data.hpp:内置参数点位表已由 HelperTimelineData.hpp 的
//   逐 tick 时间线数据替代,本文件只保留类型定义。)
// ============================================================================

namespace resources::nades {

	enum class kind : std::uint8_t
	{
		smoke,
		flash,
		molotov,
		he,
		decoy,
		// 穿点(墙bang):手持枪械时的点位,只存站位 + 穿射角,无投掷动作
		wallbang,
	};

	enum action_flag : std::uint16_t
	{
		action_none                         = 0,
		action_crouch                      = 1 << 0,
		action_run                         = 1 << 1,
		action_jump                        = 1 << 2,
		action_walk                        = 1 << 3,
		action_release_movement_after_throw = 1 << 4,
		action_always_run                  = 1 << 9,
	};
}
