#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include "nade_data.hpp"

// 运行时录制点位:字段对齐 resources::nades::entry,
// 但 name/action 用 std::string 自持有(内置数据是 const char* 静态常量,
// 录制数据必须堆上可持久化)。
struct UserLineup
{
	std::string name;
	std::string action;
	float x = 0.f, y = 0.f, z = 0.f;
	float pitch = 0.f, yaw = 0.f;
	std::uint8_t kind = 0;             // resources::nades::kind
	std::uint16_t actions = 0;         // resources::nades::action_flag 位掩码
	std::uint16_t run_ticks = 0;
	std::uint8_t after_jump_ticks = 0;
	float throw_strength = 1.f;        // 1.0=左键满力 0.0=右键轻抛 0.5=左右同按
	bool manual = false;
	// 覆盖标记:-1 = 普通录制点位;>=0 = 覆盖内置表第 N 条(收集时替代内置原条目)
	int override_builtin_index = -1;
	// 隐藏标记:仅对内置覆盖条目有意义(true = 隐藏该内置点位,不显示不参与执行)
	bool hidden = false;
};

// 用户录制点位存储:按规范化地图名分组,持久化到独立
// helper_lineups.dat(与主配置分离,.dat 避免被配置面板的 .json 扫描读到)。
class CHelperRecorder final
{
public:
	// 由 actions 位掩码生成动作标签(对齐内置点位风格:
	// Crouch+ / Walk+ / Run+ / Jump+ + Throw)。静态,录制与编辑共用。
	static std::string BuildActionLabel( std::uint16_t actions );

	// 追加一个点位到指定地图并落盘,返回该地图内的新索引。
	int Add( const std::string& mapName , const UserLineup& lineup );
	// 修改指定地图第 index 个点位并落盘,成功返回 true。
	bool Update( const std::string& mapName , std::size_t index , const UserLineup& lineup );
	// 删除指定地图第 index 个点位并落盘,成功返回 true。
	bool Remove( const std::string& mapName , std::size_t index );
	// 清空指定地图的所有点位并落盘。
	void ClearMap( const std::string& mapName );

	// 取指定地图的点位列表(无则返回 nullptr)。
	const std::vector<UserLineup>* Get( const std::string& mapName ) const;

	// 从 helper_lineups.dat 载入(启动时调用一次)。
	void Load();
	// 写回 helper_lineups.dat。
	void Save() const;

private:
	std::unordered_map<std::string , std::vector<UserLineup>> m_Maps;
};

auto GetHelperRecorder() -> CHelperRecorder*;