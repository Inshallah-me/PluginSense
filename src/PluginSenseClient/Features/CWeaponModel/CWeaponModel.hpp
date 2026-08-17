#pragma once

#include <string>
#include <vector>

// 武器模型更换:每把武器单独配置一个自定义模型(或 default 恢复原版)。
// UI: Weapon 下拉框(所有枪械 + Knife,按当前游戏语言显示)+ Model 列表(csgo\weapons 下的 .vmdl_c,首项 default)。
class CWeaponModel final
{
public:
	auto Init() -> void;
	auto OnFrame() -> void; // FrameStageNotify 每帧应用模型
	auto Shutdown() -> void;

	// 重新收集武器列表 + 重新扫描模型(菜单 Refresh 按钮用)
	auto Refresh() -> void;

public:
	auto GetWeaponNames() -> std::vector<std::string>;
	auto GetWeaponIconChars() -> std::vector<std::string>; // 每把武器的图标字符(空 = 无)
	auto GetModelNames() -> std::vector<std::string>;

	// UI 同步:切换武器时把该武器已选模型索引载入 modelSel;
	// 选择模型时把选中模型路径写回该武器。
	auto SyncWeaponToModel( int weaponSel , int& modelSel ) -> void;
	auto SyncModelToWeapon( int weaponSel , int modelSel ) -> void;

private:
	struct weapon_entry_t
	{
		std::string szName;    // 显示名(本地化/英文)
		std::string szKey;     // 短名(weapon_ 前缀去掉,如 ak47),用于图标映射
		int nDefIndex = -1;    // 物品定义索引(-1 = Knife)
		std::string szPath;    // 选中模型路径(空 = default)
	};

	struct model_entry_t
	{
		std::string szDisplay; // 显示名(相对 csgo 的资源路径,去掉 .vmdl)
		std::string szPath;    // 引擎资源路径(weapons/...),default 为空
	};

	auto CollectWeapons() -> void;
	auto ScanModels() -> void;
	auto FindEntryForWeapon( class C_BasePlayerWeapon* pWeapon ) -> weapon_entry_t*;
	auto GetWeaponModelName( class C_BaseEntity* pEntity ) -> std::string;
	auto ApplyToPawn( class C_CSPlayerPawn* pPawn ) -> void;
	auto SetModelWithCollision( class C_BaseEntity* pEntity , const std::string& path ) -> void;

private:
	std::vector<weapon_entry_t> m_entries; // 武器列表(Knife + 全部枪械)
	std::vector<model_entry_t> m_models;   // 模型列表(default + csgo\weapons)
	bool m_bInitialized = false;
};

auto GetWeaponModel() -> CWeaponModel*;
