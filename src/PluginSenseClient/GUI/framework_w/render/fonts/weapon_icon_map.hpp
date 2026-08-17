// 武器短名 -> esp_icons 图标字符(用户对照出的映射)
#pragma once

#include <string>
#include <unordered_map>

namespace weapon_icon_map
{
	static std::unordered_map<std::string , std::string> icon_table =
	{
		{ "deagle" , "\x41" } ,
		{ "elite" , "\x42" } ,
		{ "fiveseven" , "\x43" } ,
		{ "glock" , "\x44" } ,
		{ "hkp2000" , "\x45" } ,
		{ "p250" , "\x46" } ,
		{ "cz75a" , "\x49" } ,
		{ "revolver" , "\x4A" } ,
		{ "tec9" , "\x48" } ,
		{ "usp_silencer" , "\x47" } ,

		{ "mac10" , "\x4B" } ,
		{ "mp5sd" , "\x3F" } ,
		{ "mp7" , "\x4E" } ,
		{ "mp9" , "\x4F" } ,
		{ "p90" , "\x50" } ,
		{ "ump45" , "\x4C" } ,
		{ "bizon" , "\x4D" } ,

		{ "ak47" , "\x57" } ,
		{ "aug" , "\x55" } ,
		{ "famas" , "\x52" } ,
		{ "galilar" , "\x51" } ,
		{ "m4a1" , "\x53" } ,
		{ "m4a1_silencer" , "\x54" } ,
		{ "sg556" , "\x56" } ,

		{ "awp" , "\x5A" } ,
		{ "g3sg1" , "\x58" } ,
		{ "scar20" , "\x59" } ,
		{ "ssg08" , "\x5B" } ,

		{ "mag7" , "\x5E" } ,
		{ "nova" , "\x5F" } ,
		{ "sawedoff" , "\x5D" } ,
		{ "xm1014" , "\x5C" } ,

		{ "m249" , "\x61" } ,
		{ "negev" , "\x60" } ,

		{ "taser" , "\x62" } ,
		{ "knife" , "\x3A" } ,
	};
}
