#include "CSettingsJson.hpp"
#include "DllLauncher.hpp"

#include "Settings.hpp"
#include <PluginSenseClient/Settings/MenuState.hpp>
#include <PluginSenseClient/GUI/framework_w/includes.hh>
#include <PluginSenseClient/Features/CWeaponModel/CWeaponModel.hpp>

#include <filesystem>
#include <fstream>
#include <cstdio>

namespace helper
{
	extern framework::key_var_t g_helper_key;
	extern framework::key_var_t g_move_forward;
	extern framework::key_var_t g_move_back;
	extern framework::key_var_t g_move_left;
	extern framework::key_var_t g_move_right;
	extern framework::key_var_t g_move_walk;
	extern framework::key_var_t g_move_duck;
	extern framework::key_var_t g_move_jump;
	extern framework::key_var_t g_attack_key;
	extern framework::key_var_t g_attack2_key;
}

static CSettingsJson g_CSettingsJson{};

auto CSettingsJson::LoadConfig( const std::string& JsonFile ) -> void
{
	std::filesystem::create_directories( GetConfigDir() );

	auto ConfigLoadedIndex = 0u;
	const auto ConfigFilePath = GetConfigDir() + JsonFile;

	for ( const auto& Config : m_vecConfigList )
	{
		if ( Config == JsonFile )
		{
			m_nConfigLoadedIndex = ConfigLoadedIndex;
			break;
		}

		ConfigLoadedIndex++;
	}

	std::ifstream ConfigFile( ConfigFilePath );

	rapidjson::IStreamWrapper StreamWrapper( ConfigFile );
	rapidjson::Document DocumentConfig;

	DocumentConfig.ParseStream( StreamWrapper );

	if ( !DocumentConfig.HasParseError() && DocumentConfig.HasMember( "Settings" ) )
	{
		const auto& SettingsJson = DocumentConfig["Settings"];
		GetIntJson( SettingsJson , "menu_key" , vars::menuKey , 1 , 255 );
		GetBoolJson( SettingsJson , "menu_keybinds" , vars::menuKeybinds );
		GetIntJson( SettingsJson , "helper_key" , helper::g_helper_key.key , 0 , 255 );
		GetIntJson( SettingsJson , "move_forward" , helper::g_move_forward.key , 0 , 255 );
		GetIntJson( SettingsJson , "move_back" , helper::g_move_back.key , 0 , 255 );
		GetIntJson( SettingsJson , "move_left" , helper::g_move_left.key , 0 , 255 );
		GetIntJson( SettingsJson , "move_right" , helper::g_move_right.key , 0 , 255 );
		GetIntJson( SettingsJson , "move_walk" , helper::g_move_walk.key , 0 , 255 );
		GetIntJson( SettingsJson , "move_crouch" , helper::g_move_duck.key , 0 , 255 );
		GetIntJson( SettingsJson , "move_jump" , helper::g_move_jump.key , 0 , 255 );
		GetIntJson( SettingsJson , "throw_key" , helper::g_attack_key.key , 0 , 255 );
		GetIntJson( SettingsJson , "throw2_key" , helper::g_attack2_key.key , 0 , 255 );
		// helper 配置
		GetBoolJson( SettingsJson , "helper_enabled" , menu_state::helperEnabled );
		GetBoolJson( SettingsJson , "auto_move" , menu_state::autoMove );
		GetBoolJson( SettingsJson , "auto_aim" , menu_state::autoAim );
		GetBoolJson( SettingsJson , "auto_execute" , menu_state::autoExecute );
		GetIntJson( SettingsJson , "aim_speed" , menu_state::aimSpeed , 1 , 30 );
		GetFloatJson( SettingsJson , "aim_threshold" , menu_state::aimThreshold , 0.05f , 3.f );
		GetIntJson( SettingsJson , "lock_time_ms" , menu_state::lockTimeMs , 0 , 250 );
		GetIntJson( SettingsJson , "draw_distance" , menu_state::drawDistance , 100 , 2000 );
		GetIntJson( SettingsJson , "stand_distance" , menu_state::standDistance , 50 , 600 );
		GetFloatJson( SettingsJson , "stand_radius" , menu_state::standRadius , 1.f , 100.f );
		GetFloatJson( SettingsJson , "release_radius" , menu_state::releaseRadius , 1.f , 20.f );
		GetFloatJson( SettingsJson , "height_tolerance" , menu_state::heightTolerance , 1.f , 32.f );
		GetBoolJson( SettingsJson , "show_action" , menu_state::showAction );
		GetBoolJson( SettingsJson , "show_distance" , menu_state::showDistance );
		GetIntJson( SettingsJson , "menu_animation_speed" , vars::menuAnimSpeed , 0 , 100 );
		GetColorJson( SettingsJson , "menu_accent" , vars::colorAccent );
		GetIntJson( SettingsJson , "menu_outline_alpha" , style::outlineAlpha , 0 , 255 );
		GetFloatJson( SettingsJson , "menu_board_opacity" , style::glassWindow , 0.f , 1.f );
		GetFloatJson( SettingsJson , "menu_panel_opacity" , style::glassPanel , 0.f , 1.f );
		GetFloatJson( SettingsJson , "menu_widget_opacity" , style::glassWidget , 0.f , 1.f );
		GetFloatJson( SettingsJson , "menu_popup_opacity" , style::glassPopup , 0.f , 1.f );
		GetBoolJson( SettingsJson, "clantag_enabled", menu_state::clantagEnabled );
		GetIntJson( SettingsJson, "clantag_selection", menu_state::clantagSelection, 0, 22 );
		GetFloatJson( SettingsJson, "clantag_speed", menu_state::clantagSpeed, .1f, 1.f );
		GetBoolJson( SettingsJson, "clantag_animated", menu_state::animatedClantag );
		GetIntJson( SettingsJson, "clantag_animation", menu_state::animationSelection, 0, 19 );
		GetTextJson( SettingsJson, "player_name", menu_state::playerName, sizeof(menu_state::playerName) );
		GetTextJson( SettingsJson, "custom_clantag", menu_state::customClantag, sizeof(menu_state::customClantag) );
		GetBoolJson( SettingsJson, "velocity_text", menu_state::velocityText );
		GetBoolJson( SettingsJson, "velocity_graph", menu_state::velocityGraph );
		GetBoolJson( SettingsJson, "keystrokes", menu_state::keystrokes );
		GetFloatJson( SettingsJson, "velocity_offset", menu_state::velocityOffset, 30.f, 250.f );
		GetBoolJson( SettingsJson, "damage_log_enabled", menu_state::damageLogEnabled );
		GetBoolJson( SettingsJson, "hitlog_enabled", menu_state::hitlogEnabled );
		GetBoolJson( SettingsJson, "hitlog_victim", menu_state::hitlogVictim );
		GetIntJson( SettingsJson, "hitlog_type", menu_state::hitlogType, 0, 1 );
		GetBoolJson( SettingsJson, "damage_indicator", menu_state::damageIndicator );
		GetColorJson( SettingsJson, "velocity_low", &menu_state::lowSpeed.x ); GetColorJson( SettingsJson, "velocity_mid", &menu_state::midSpeed.x ); GetColorJson( SettingsJson, "velocity_high", &menu_state::highSpeed.x );
		GetColorJson( SettingsJson, "velocity_graph_color", &menu_state::graphColor.x ); GetColorJson( SettingsJson, "damage_body", &menu_state::damageBody.x ); GetColorJson( SettingsJson, "damage_head", &menu_state::damageHead.x );
			// World visuals
			GetBoolJson( SettingsJson, "dof", menu_state::worldScene.dof );
			GetBoolJson( SettingsJson, "dof_focus", menu_state::worldScene.dofFocus );
			GetFloatJson( SettingsJson, "dof_near_blurry", menu_state::worldScene.dofNearBlurry, 0.f, 50.f );
			GetFloatJson( SettingsJson, "dof_near_crisp", menu_state::worldScene.dofNearCrisp, 0.f, 100.f );
			GetFloatJson( SettingsJson, "dof_far_crisp", menu_state::worldScene.dofFarCrisp, 100.f, 2000.f );
			GetFloatJson( SettingsJson, "dof_far_blurry", menu_state::worldScene.dofFarBlurry, 200.f, 5000.f );
			GetBoolJson( SettingsJson, "bloom", menu_state::worldScene.bloom );
			GetFloatJson( SettingsJson, "bloom_value", menu_state::worldScene.bloomValue, 0.f, 3.f );
			GetBoolJson( SettingsJson, "gamma", menu_state::worldScene.gamma );
			GetFloatJson( SettingsJson, "gamma_value", menu_state::worldScene.gammaValue, 0.5f, 5.f );
				GetBoolJson( SettingsJson, "motion_camera", menu_state::worldScene.motionCamera );
				GetFloatJson( SettingsJson, "cam_hor_offset", menu_state::worldScene.camHorOffset, -30.f, 30.f );
				GetFloatJson( SettingsJson, "cam_ver_offset", menu_state::worldScene.camVerOffset, -50.f, 50.f );
				GetFloatJson( SettingsJson, "cam_slack", menu_state::worldScene.camSlack, 1.f, 100.f );
				GetBoolJson( SettingsJson, "cam_crosshair", menu_state::worldScene.camCrosshair );
				GetColorJson( SettingsJson, "cam_crosshair_color", &menu_state::worldScene.camCrosshairColor.x );
				GetFloatJson( SettingsJson, "cam_crosshair_length", menu_state::worldScene.camCrosshairLength, 4.f, 20.f );
				GetFloatJson( SettingsJson, "cam_crosshair_thickness", menu_state::worldScene.camCrosshairThickness, 1.f, 4.f );
			GetBoolJson( SettingsJson, "wind", menu_state::worldWeather.wind );
			GetFloatJson( SettingsJson, "wind_strength", menu_state::worldWeather.windStrength, 0.f, 10.f );
			GetFloatJson( SettingsJson, "wind_direction", menu_state::worldWeather.windDirection, 0.f, 360.f );
			GetFloatJson( SettingsJson, "wind_turbulence", menu_state::worldWeather.windTurbulence, 0.f, 5.f );
			GetBoolJson( SettingsJson, "weather", menu_state::worldWeather.enabled );
			GetIntJson( SettingsJson, "weather_type", menu_state::worldWeather.type, 0, 2 );
			GetColorJson( SettingsJson, "weather_color", &menu_state::worldWeather.color.x );
			GetBoolJson( SettingsJson, "wetness", menu_state::worldWeather.wetness );
			GetFloatJson( SettingsJson, "wetness_density", menu_state::worldWeather.wetnessDensity, 0.f, 5.f );
			GetFloatJson( SettingsJson, "wetness_speed", menu_state::worldWeather.wetnessSpeed, 0.1f, 3.f );
		GetFloatJson( SettingsJson, "damage_scale", menu_state::damageScale, .25f, 2.f ); GetFloatJson( SettingsJson, "damage_time", menu_state::damageTime, 1.f, 10.f );
		GetBoolJson( SettingsJson, "spoof_enabled", menu_state::spoof ); GetBoolJson( SettingsJson, "cooldown_enabled", menu_state::fakeCooldown ); GetBoolJson( SettingsJson, "official_ban", menu_state::officialBan ); GetBoolJson( SettingsJson, "vac_ban", menu_state::vacBan ); GetIntJson( SettingsJson, "cooldown_type", menu_state::fakeCooldownValue, 0, 19 ); GetIntJson( SettingsJson, "cooldown_time", menu_state::fakeCooldownTime, 0, 7 ); GetIntJson( SettingsJson, "cooldown_custom_days", menu_state::fakeCooldownCustomDays, 1, 9999 );
		GetBoolJson( SettingsJson, "lobby_premier", menu_state::lobbyPremier ); GetIntJson( SettingsJson, "lobby_premier_rating", menu_state::lobbyPremierRating, 0, 99999 ); GetIntJson( SettingsJson, "lobby_premier_wins", menu_state::lobbyPremierWins, 0, 9999 ); GetBoolJson( SettingsJson, "lobby_wingman", menu_state::lobbyWingman ); GetIntJson( SettingsJson, "lobby_wingman_rating", menu_state::lobbyWingmanRating, 0, 99999 ); GetIntJson( SettingsJson, "lobby_wingman_wins", menu_state::lobbyWingmanWins, 0, 9999 ); GetBoolJson( SettingsJson, "lobby_level", menu_state::lobbyLevel ); GetIntJson( SettingsJson, "lobby_level_value", menu_state::lobbyLevelValue, 0, 40 ); GetIntJson( SettingsJson, "lobby_xp", menu_state::lobbyXp, 0, 5000 ); GetBoolJson( SettingsJson, "lobby_prime", menu_state::lobbyPrime );
		GetBoolJson( SettingsJson, "chat_spammer", menu_state::chatSpammer ); GetBoolJson( SettingsJson, "kill_say", menu_state::killSay ); GetBoolJson( SettingsJson, "vacnet_enabled", menu_state::vacnetEnabled ); GetFloatJson( SettingsJson, "chat_delay", menu_state::sendDelay, .1f, 20.f );
		GetIntJson( SettingsJson, "chat_count", menu_state::chatCount, 1, 16 ); GetIntJson( SettingsJson, "kill_count", menu_state::killCount, 1, 16 );
		for (int i = 0; i < 16; ++i) { char key[32]{}; std::snprintf(key, sizeof(key), "chat_message_%d", i); GetTextJson(SettingsJson, key, menu_state::chatMessages[i], 128); std::snprintf(key, sizeof(key), "kill_message_%d", i); GetTextJson(SettingsJson, key, menu_state::killMessages[i], 128); }
		GetBoolJson( SettingsJson, "weapon_model_enabled", menu_state::weaponModelEnabled ); GetIntJson( SettingsJson, "weapon_model_weapon", menu_state::weaponModelWeapon, 0, 256 ); GetIntJson( SettingsJson, "weapon_model_model", menu_state::weaponModelModel, 0, 2048 );
		// 每把武器的模型路径(配置持久化)
		{
			const auto weaponNames = GetWeaponModel()->GetWeaponNames();
			for ( std::size_t i = 0; i < weaponNames.size(); ++i )
			{
				const std::string key = "weapon_model_path_" + std::to_string( i );
				char buf[ 256 ] = {};
				GetTextJson( SettingsJson, key.c_str(), buf, sizeof( buf ) );
				if ( buf[ 0 ] != '\0' )
					GetWeaponModel()->SetWeaponModelPath( static_cast<int>( i ), buf );
			}
		}
	}
	else
	{
		DEV_LOG( "[error] LoadConfig: %s -> %s , %i\n" , ConfigFilePath.c_str() , rapidjson::GetParseError_En( DocumentConfig.GetParseError() ), DocumentConfig.GetErrorOffset() );
	}

	DocumentConfig.Clear();
	ConfigFile.close();
}

auto CSettingsJson::SaveConfig( const std::string& JsonFile ) -> void
{
	const auto ConfigFilePath = GetConfigDir() + JsonFile;

	std::ofstream ConfigFile( ConfigFilePath );

	rapidjson::OStreamWrapper StreamWrapper( ConfigFile );
	rapidjson::PrettyWriter<rapidjson::OStreamWrapper> ConfigWriter( StreamWrapper );
	
	ConfigWriter.SetIndent( '\t' , 1 );
	ConfigWriter.SetFormatOptions( rapidjson::PrettyFormatOptions::kFormatSingleLineArray );
	ConfigWriter.SetMaxDecimalPlaces( 2 );

	ConfigWriter.StartObject();
	{
		ConfigWriter.String( XorStr( "Settings" ) );
		{
			ConfigWriter.StartObject();
			{
AddIntJson( ConfigWriter , "menu_key" , vars::menuKey );
AddBoolJson( ConfigWriter , "menu_keybinds" , vars::menuKeybinds );
AddIntJson( ConfigWriter , "helper_key" , helper::g_helper_key.key );
AddIntJson( ConfigWriter , "move_forward" , helper::g_move_forward.key );
AddIntJson( ConfigWriter , "move_back" , helper::g_move_back.key );
AddIntJson( ConfigWriter , "move_left" , helper::g_move_left.key );
AddIntJson( ConfigWriter , "move_right" , helper::g_move_right.key );
AddIntJson( ConfigWriter , "move_walk" , helper::g_move_walk.key );
AddIntJson( ConfigWriter , "move_crouch" , helper::g_move_duck.key );
AddIntJson( ConfigWriter , "move_jump" , helper::g_move_jump.key );
AddIntJson( ConfigWriter , "throw_key" , helper::g_attack_key.key );
AddIntJson( ConfigWriter , "throw2_key" , helper::g_attack2_key.key );
// helper 配置
AddBoolJson( ConfigWriter , "helper_enabled" , menu_state::helperEnabled );
AddBoolJson( ConfigWriter , "auto_move" , menu_state::autoMove );
AddBoolJson( ConfigWriter , "auto_aim" , menu_state::autoAim );
AddBoolJson( ConfigWriter , "auto_execute" , menu_state::autoExecute );
AddIntJson( ConfigWriter , "aim_speed" , menu_state::aimSpeed );
AddFloatJson( ConfigWriter , "aim_threshold" , menu_state::aimThreshold );
AddIntJson( ConfigWriter , "lock_time_ms" , menu_state::lockTimeMs );
AddIntJson( ConfigWriter , "draw_distance" , menu_state::drawDistance );
AddIntJson( ConfigWriter , "stand_distance" , menu_state::standDistance );
AddFloatJson( ConfigWriter , "stand_radius" , menu_state::standRadius );
AddFloatJson( ConfigWriter , "release_radius" , menu_state::releaseRadius );
AddFloatJson( ConfigWriter , "height_tolerance" , menu_state::heightTolerance );
AddBoolJson( ConfigWriter , "show_action" , menu_state::showAction );
AddBoolJson( ConfigWriter , "show_distance" , menu_state::showDistance );
AddIntJson( ConfigWriter , "menu_animation_speed" , vars::menuAnimSpeed );
AddColorJson( ConfigWriter , "menu_accent" , vars::colorAccent );
AddIntJson( ConfigWriter , "menu_outline_alpha" , style::outlineAlpha );
AddFloatJson( ConfigWriter , "menu_board_opacity" , style::glassWindow );
AddFloatJson( ConfigWriter , "menu_panel_opacity" , style::glassPanel );
AddFloatJson( ConfigWriter , "menu_widget_opacity" , style::glassWidget );
AddFloatJson( ConfigWriter , "menu_popup_opacity" , style::glassPopup );
AddBoolJson(ConfigWriter, "clantag_enabled", menu_state::clantagEnabled); AddIntJson(ConfigWriter, "clantag_selection", menu_state::clantagSelection); AddFloatJson(ConfigWriter, "clantag_speed", menu_state::clantagSpeed); AddBoolJson(ConfigWriter, "clantag_animated", menu_state::animatedClantag); AddIntJson(ConfigWriter, "clantag_animation", menu_state::animationSelection);
AddTextJson(ConfigWriter, "player_name", menu_state::playerName); AddTextJson(ConfigWriter, "custom_clantag", menu_state::customClantag);
AddBoolJson(ConfigWriter, "velocity_text", menu_state::velocityText); AddBoolJson(ConfigWriter, "velocity_graph", menu_state::velocityGraph); AddBoolJson(ConfigWriter, "keystrokes", menu_state::keystrokes); AddFloatJson(ConfigWriter, "velocity_offset", menu_state::velocityOffset); AddBoolJson(ConfigWriter, "damage_log_enabled", menu_state::damageLogEnabled); AddBoolJson(ConfigWriter, "hitlog_enabled", menu_state::hitlogEnabled); AddBoolJson(ConfigWriter, "hitlog_victim", menu_state::hitlogVictim); AddIntJson(ConfigWriter, "hitlog_type", menu_state::hitlogType); AddBoolJson(ConfigWriter, "damage_indicator", menu_state::damageIndicator);
AddColorJson(ConfigWriter, "velocity_low", &menu_state::lowSpeed.x); AddColorJson(ConfigWriter, "velocity_mid", &menu_state::midSpeed.x); AddColorJson(ConfigWriter, "velocity_high", &menu_state::highSpeed.x); AddColorJson(ConfigWriter, "velocity_graph_color", &menu_state::graphColor.x); AddColorJson(ConfigWriter, "damage_body", &menu_state::damageBody.x); AddColorJson(ConfigWriter, "damage_head", &menu_state::damageHead.x);
// World visuals
AddBoolJson(ConfigWriter, "dof", menu_state::worldScene.dof);
			AddBoolJson(ConfigWriter, "dof_focus", menu_state::worldScene.dofFocus);
AddFloatJson(ConfigWriter, "dof_near_blurry", menu_state::worldScene.dofNearBlurry);
AddFloatJson(ConfigWriter, "dof_near_crisp", menu_state::worldScene.dofNearCrisp);
AddFloatJson(ConfigWriter, "dof_far_crisp", menu_state::worldScene.dofFarCrisp);
AddFloatJson(ConfigWriter, "dof_far_blurry", menu_state::worldScene.dofFarBlurry);
AddBoolJson(ConfigWriter, "bloom", menu_state::worldScene.bloom);
AddFloatJson(ConfigWriter, "bloom_value", menu_state::worldScene.bloomValue);
AddBoolJson(ConfigWriter, "gamma", menu_state::worldScene.gamma);
AddFloatJson(ConfigWriter, "gamma_value", menu_state::worldScene.gammaValue);
				AddBoolJson(ConfigWriter, "motion_camera", menu_state::worldScene.motionCamera);
				AddFloatJson(ConfigWriter, "cam_hor_offset", menu_state::worldScene.camHorOffset);
				AddFloatJson(ConfigWriter, "cam_ver_offset", menu_state::worldScene.camVerOffset);
				AddFloatJson(ConfigWriter, "cam_slack", menu_state::worldScene.camSlack);
				AddBoolJson(ConfigWriter, "cam_crosshair", menu_state::worldScene.camCrosshair);
				AddColorJson(ConfigWriter, "cam_crosshair_color", &menu_state::worldScene.camCrosshairColor.x);
				AddFloatJson(ConfigWriter, "cam_crosshair_length", menu_state::worldScene.camCrosshairLength);
				AddFloatJson(ConfigWriter, "cam_crosshair_thickness", menu_state::worldScene.camCrosshairThickness);
AddBoolJson(ConfigWriter, "wind", menu_state::worldWeather.wind);
AddFloatJson(ConfigWriter, "wind_strength", menu_state::worldWeather.windStrength);
AddFloatJson(ConfigWriter, "wind_direction", menu_state::worldWeather.windDirection);
AddFloatJson(ConfigWriter, "wind_turbulence", menu_state::worldWeather.windTurbulence);
AddBoolJson(ConfigWriter, "weather", menu_state::worldWeather.enabled);
AddIntJson(ConfigWriter, "weather_type", menu_state::worldWeather.type);
AddColorJson(ConfigWriter, "weather_color", &menu_state::worldWeather.color.x);
AddBoolJson(ConfigWriter, "wetness", menu_state::worldWeather.wetness);
AddFloatJson(ConfigWriter, "wetness_density", menu_state::worldWeather.wetnessDensity);
AddFloatJson(ConfigWriter, "wetness_speed", menu_state::worldWeather.wetnessSpeed);
			AddFloatJson(ConfigWriter, "damage_scale", menu_state::damageScale); AddFloatJson(ConfigWriter, "damage_time", menu_state::damageTime);
AddBoolJson(ConfigWriter, "spoof_enabled", menu_state::spoof); AddBoolJson(ConfigWriter, "cooldown_enabled", menu_state::fakeCooldown); AddBoolJson(ConfigWriter, "official_ban", menu_state::officialBan); AddBoolJson(ConfigWriter, "vac_ban", menu_state::vacBan); AddIntJson(ConfigWriter, "cooldown_type", menu_state::fakeCooldownValue); AddIntJson(ConfigWriter, "cooldown_time", menu_state::fakeCooldownTime); AddIntJson(ConfigWriter, "cooldown_custom_days", menu_state::fakeCooldownCustomDays);
AddBoolJson(ConfigWriter, "lobby_premier", menu_state::lobbyPremier); AddIntJson(ConfigWriter, "lobby_premier_rating", menu_state::lobbyPremierRating); AddIntJson(ConfigWriter, "lobby_premier_wins", menu_state::lobbyPremierWins); AddBoolJson(ConfigWriter, "lobby_wingman", menu_state::lobbyWingman); AddIntJson(ConfigWriter, "lobby_wingman_rating", menu_state::lobbyWingmanRating); AddIntJson(ConfigWriter, "lobby_wingman_wins", menu_state::lobbyWingmanWins); AddBoolJson(ConfigWriter, "lobby_level", menu_state::lobbyLevel); AddIntJson(ConfigWriter, "lobby_level_value", menu_state::lobbyLevelValue); AddIntJson(ConfigWriter, "lobby_xp", menu_state::lobbyXp); AddBoolJson(ConfigWriter, "lobby_prime", menu_state::lobbyPrime);
AddBoolJson(ConfigWriter, "chat_spammer", menu_state::chatSpammer); AddBoolJson(ConfigWriter, "kill_say", menu_state::killSay); AddBoolJson(ConfigWriter, "vacnet_enabled", menu_state::vacnetEnabled); AddFloatJson(ConfigWriter, "chat_delay", menu_state::sendDelay); AddIntJson(ConfigWriter, "chat_count", menu_state::chatCount); AddIntJson(ConfigWriter, "kill_count", menu_state::killCount);
for (int i = 0; i < 16; ++i) { char key[32]{}; std::snprintf(key, sizeof(key), "chat_message_%d", i); AddTextJson(ConfigWriter, key, menu_state::chatMessages[i]); std::snprintf(key, sizeof(key), "kill_message_%d", i); AddTextJson(ConfigWriter, key, menu_state::killMessages[i]); }
AddBoolJson(ConfigWriter, "weapon_model_enabled", menu_state::weaponModelEnabled); AddIntJson(ConfigWriter, "weapon_model_weapon", menu_state::weaponModelWeapon); AddIntJson(ConfigWriter, "weapon_model_model", menu_state::weaponModelModel);
// 每把武器的模型路径(配置持久化)
{
	const auto weaponNames = GetWeaponModel()->GetWeaponNames();
	for ( std::size_t i = 0; i < weaponNames.size(); ++i )
	{
		const std::string key = "weapon_model_path_" + std::to_string( i );
		AddTextJson( ConfigWriter, key.c_str(), GetWeaponModel()->GetWeaponModelPath( static_cast<int>( i ) ).c_str() );
	}
}

			}
			ConfigWriter.EndObject();
		}
	}
	ConfigWriter.EndObject();
	
	ConfigFile.close();
}

auto CSettingsJson::DeleteConfig( const std::string& JsonFile ) -> void
{
	const auto ConfigFilePath = GetConfigDir() + JsonFile;

	DeleteFileA( ConfigFilePath.c_str() );
}

auto CSettingsJson::UpdateConfigList() -> void
{
	m_vecConfigList.clear();

	std::filesystem::create_directories( GetConfigDir() );

	if ( !std::filesystem::exists( GetConfigDir() ) )
		return;

	for ( const auto& Entry : std::filesystem::directory_iterator( GetConfigDir().c_str() ) )
	{
		if ( Entry.is_regular_file() )
		{
			if ( Entry.path().extension().string() == XorStr( ".json" ) )
m_vecConfigList.emplace_back( Entry.path().filename().string() );
		}
	}
}

auto CSettingsJson::GetIntJson( const rapidjson::Value& JsonValue , const char* Name , int& Output , const int Min , const int Max ) -> void
{
	if ( !JsonValue.IsNull() && JsonValue.HasMember( Name ) )
	{
		auto& Value = JsonValue[Name];

		if ( !Value.IsNull() && Value.IsInt() )
		{
			const auto IntValue = Value.GetInt();

			if ( IntValue < Min )
Output = Min;
			else if ( IntValue > Max )
Output = Max;
			else
Output = IntValue;
		}
	}
}

auto CSettingsJson::GetBoolJson( const rapidjson::Value& JsonValue , const char* Name , bool& Output ) -> void
{
	if ( !JsonValue.IsNull() && JsonValue.HasMember( Name ) )
	{
		auto& Value = JsonValue[Name];

		if ( !Value.IsNull() && Value.IsBool() )
			Output = Value.GetBool();
	}
}

auto CSettingsJson::GetFloatJson( const rapidjson::Value& JsonValue , const char* Name , float& Output , const float Min , const float Max ) -> void
{
	if ( !JsonValue.IsNull() && JsonValue.HasMember( Name ) )
	{
		auto& Value = JsonValue[Name];

		if ( !Value.IsNull() && Value.IsFloat() )
		{
			Output = std::clamp( Value.GetFloat() , Min , Max );
		}
	}
}

auto CSettingsJson::GetColorJson( const rapidjson::Value& JsonValue , const char* Name , float* Output ) -> void
{
	if ( !JsonValue.IsNull() && JsonValue.HasMember( Name ) )
	{
		auto& Value = JsonValue[Name];

		if ( !Value.IsNull() && Value.IsArray() && Value.GetArray().Size() == 4 )
		{
			Output[0] = Value.GetArray()[0].GetFloat();
			Output[1] = Value.GetArray()[1].GetFloat();
			Output[2] = Value.GetArray()[2].GetFloat();
			Output[3] = Value.GetArray()[3].GetFloat();

			Output[0] = std::clamp( Output[0] , 0.f , 1.f );
			Output[1] = std::clamp( Output[1] , 0.f , 1.f );
			Output[2] = std::clamp( Output[2] , 0.f , 1.f );
			Output[3] = std::clamp( Output[3] , 0.f , 1.f );
		}
	}
}

auto CSettingsJson::GetStringJson( const rapidjson::Value& JsonValue , const char* Name , std::string& Output ) -> void
{
	if ( !JsonValue.IsNull() && JsonValue.HasMember( Name ) )
	{
		auto& Value = JsonValue[Name];

		if ( Value.IsString() )
		{
			Output = Value.GetString();
		}
	}
}

auto CSettingsJson::GetTextJson( const rapidjson::Value& JsonValue, const char* Name, char* Output, size_t OutputSize ) -> void
{
	if (OutputSize && !JsonValue.IsNull() && JsonValue.HasMember(Name) && JsonValue[Name].IsString())
		std::snprintf(Output, OutputSize, "%s", JsonValue[Name].GetString());
}

auto CSettingsJson::GetBoneSelectedJson( const rapidjson::Value& JsonValue , const char* Name , bool* Output ) -> void
{
	if ( !JsonValue.IsNull() && JsonValue.HasMember( Name ) )
	{
		auto& Value = JsonValue[Name];

		if ( !Value.IsNull() && Value.IsArray() && Value.GetArray().Size() == 4 )
		{
			Output[0] = Value.GetArray()[0].GetBool();
			Output[1] = Value.GetArray()[1].GetBool();
			Output[2] = Value.GetArray()[2].GetBool();
			Output[3] = Value.GetArray()[3].GetBool();
		}
	}
}

auto CSettingsJson::AddIntJson( rapidjson::PrettyWriter<rapidjson::OStreamWrapper>& Writer , const char* Name , const int& Output ) -> void
{
	Writer.String( Name );
	Writer.Int( Output );
}

auto CSettingsJson::AddUInt64Json( rapidjson::PrettyWriter<rapidjson::OStreamWrapper>& Writer , const char* Name , const uint64_t& Output ) -> void
{
	Writer.String( Name );
	Writer.Uint64( Output );
}

auto CSettingsJson::AddBoolJson( rapidjson::PrettyWriter<rapidjson::OStreamWrapper>& Writer , const char* Name , const bool& Output ) -> void
{
	Writer.String( Name );
	Writer.Bool( Output );
}

auto CSettingsJson::AddStringJson( rapidjson::PrettyWriter<rapidjson::OStreamWrapper>& Writer , const char* Name , const std::string& Output ) -> void
{
	Writer.String( Name );
	Writer.String( Output.c_str() );
}

auto CSettingsJson::AddTextJson( rapidjson::PrettyWriter<rapidjson::OStreamWrapper>& Writer, const char* Name, const char* Output ) -> void
{
	Writer.String(Name);
	Writer.String(Output ? Output : "");
}

auto CSettingsJson::AddFloatJson( rapidjson::PrettyWriter<rapidjson::OStreamWrapper>& Writer , const char* Name , const float& Output ) -> void
{
	Writer.String( Name );
	Writer.Double( static_cast<double>( Output ) );
}

auto CSettingsJson::AddColorJson( rapidjson::PrettyWriter<rapidjson::OStreamWrapper>& Writer , const char* Name , const float* Output ) -> void
{
	Writer.String( Name );
	Writer.StartArray();

	Writer.Double( static_cast<double>( Output[0] ) );
	Writer.Double( static_cast<double>( Output[1] ) );
	Writer.Double( static_cast<double>( Output[2] ) );
	Writer.Double( static_cast<double>( Output[3] ) );

	Writer.EndArray();
}

auto CSettingsJson::AddBoneSelectedJson( rapidjson::PrettyWriter<rapidjson::OStreamWrapper>& Writer , const char* Name , const bool* Output ) -> void
{
	Writer.String( Name );
	Writer.StartArray();

	Writer.Bool( Output[0] );
	Writer.Bool( Output[1] );
	Writer.Bool( Output[2] );
	Writer.Bool( Output[3] );

	Writer.EndArray();
}

auto GetSettingsJson() -> CSettingsJson*
{
	return &g_CSettingsJson;
}
