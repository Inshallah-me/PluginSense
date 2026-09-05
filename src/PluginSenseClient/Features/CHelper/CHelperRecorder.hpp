#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include <unordered_map>

#include "HelperTimeline.hpp"
#include "NadeKinds.hpp"

// ============================================================================
// 用户自录点位存储(v2,时间线格式):
//   - 雷类条目:frames 非空,逐 usercmd 的按钮/视角/位置,回放走时间线引擎
//   - 墙点条目:frames 为空,纯站位快照(位置 + 视角 + 可用枪列表)
// 持久化到 helper_lineups.dat(与主配置分离)。
// ============================================================================
struct UserLineup
{
	std::string name;
	std::string weapon;    // 墙点:可用枪短名列表(逗号分隔);非墙点为空
	float x = 0.f, y = 0.f, z = 0.f;
	float pitch = 0.f, yaw = 0.f;
	std::uint8_t kind = 0;             // resources::nades::kind
	bool hidden = false;
	std::uint8_t annotations = 0;      // 墙点标注位(仅 wallbang):1=Crouch 4=Jump(action_flag 同值)
	// 内置点位库的隐藏覆盖标记:>=0 = 只表示"隐藏内置库第 id 条"(无其它数据)
	int builtin_id = -1;
	std::vector<helper_timeline::Frame> frames; // 非空 = 时间线点位
};

class CHelperRecorder final
{
public:
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

	// 内部表访问(时间线库加载时读取隐藏覆盖)
	auto MutableMaps() -> std::unordered_map<std::string , std::vector<UserLineup>>& { return m_Maps; }

private:
	std::unordered_map<std::string , std::vector<UserLineup>> m_Maps;
};

auto GetHelperRecorder() -> CHelperRecorder*;
