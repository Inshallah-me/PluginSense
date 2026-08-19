#include "../includes.hh"

#include <algorithm>
#include <array>
#include <cstdio>
#include <string>
#include <vector>

#include <PluginSenseClient/Settings/CSettingsJson.hpp>
#include <PluginSenseClient/Settings/MenuState.hpp>
#include <PluginSenseClient/Settings/Settings.hpp>
#include <PluginSenseClient/Features/CNameChanger/CNameChanger.hpp>
#include <PluginSenseClient/Features/CWeaponModel/CWeaponModel.hpp>

#define half_height 255
#define full_height 525
#define ICON_FA_VISUAL "\xef\x81\xae" // U+F06E
#define ICON_FA_MODEL "\xef\x96\xae" // U+F5AE

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

namespace
{
	hue::c_color g_velocity_low{};
	hue::c_color g_velocity_mid{};
	hue::c_color g_velocity_high{};
	hue::c_color g_velocity_graph{};
	hue::c_color g_damage_body{};
	hue::c_color g_damage_head{};
	hue::c_color g_weather_color{ 255, 255, 255, 255 };
	hue::c_color g_crosshair_color{ 255, 255, 0, 255 };
	hue::c_color g_menu_accent{ 157, 196, 29, 255 };

	std::array<std::string, 16> g_chat_messages{};
	std::array<std::string, 16> g_kill_messages{};
	std::string g_player_name{};
	std::string g_custom_clantag{};
	std::string g_config_name{};
	std::vector<std::string> g_config_items{ "Empty" };
	int g_config_index{};
	bool g_text_cache_ready{};
	framework::key_var_t g_menu_key{};

	hue::c_color to_hue(const ImVec4& color)
	{
		return hue::c_color(
			static_cast<int>(std::clamp(color.x, 0.f, 1.f) * 255.f),
			static_cast<int>(std::clamp(color.y, 0.f, 1.f) * 255.f),
			static_cast<int>(std::clamp(color.z, 0.f, 1.f) * 255.f),
			static_cast<int>(std::clamp(color.w, 0.f, 1.f) * 255.f));
	}

	bool keybind_is_bound(const framework::key_var_t& keybind);

	void to_imvec4(const hue::c_color& color, ImVec4& out)
	{
		out = ImVec4(
			std::clamp(color.r, 0, 255) / 255.f,
			std::clamp(color.g, 0, 255) / 255.f,
			std::clamp(color.b, 0, 255) / 255.f,
			std::clamp(color.a, 0, 255) / 255.f);
	}

	hue::c_color to_hue(const float color[4])
	{
		return hue::c_color(
			static_cast<int>(std::clamp(color[0], 0.f, 1.f) * 255.f),
			static_cast<int>(std::clamp(color[1], 0.f, 1.f) * 255.f),
			static_cast<int>(std::clamp(color[2], 0.f, 1.f) * 255.f),
			static_cast<int>(std::clamp(color[3], 0.f, 1.f) * 255.f));
	}

	void to_float_color(const hue::c_color& color, float out[4])
	{
		out[0] = std::clamp(color.r, 0, 255) / 255.f;
		out[1] = std::clamp(color.g, 0, 255) / 255.f;
		out[2] = std::clamp(color.b, 0, 255) / 255.f;
		out[3] = std::clamp(color.a, 0, 255) / 255.f;
	}

	void apply_menu_accent()
	{
		framework::g_style->m_accent = g_menu_accent;
	}

	void refresh_color_cache_from_settings()
	{
		g_menu_accent = to_hue(vars::colorAccent);
		g_velocity_low = to_hue(menu_state::lowSpeed);
		g_velocity_mid = to_hue(menu_state::midSpeed);
		g_velocity_high = to_hue(menu_state::highSpeed);
		g_velocity_graph = to_hue(menu_state::graphColor);
		g_damage_body = to_hue(menu_state::damageBody);
		g_damage_head = to_hue(menu_state::damageHead);
		g_weather_color = to_hue(menu_state::worldWeather.color);
		g_crosshair_color = to_hue(menu_state::worldScene.camCrosshairColor);

		apply_menu_accent();
	}

	void push_color_cache_to_settings()
	{
		to_float_color(g_menu_accent, vars::colorAccent);
		to_imvec4(g_velocity_low, menu_state::lowSpeed);
		to_imvec4(g_velocity_mid, menu_state::midSpeed);
		to_imvec4(g_velocity_high, menu_state::highSpeed);
		to_imvec4(g_velocity_graph, menu_state::graphColor);
		to_imvec4(g_damage_body, menu_state::damageBody);
		to_imvec4(g_damage_head, menu_state::damageHead);
		to_imvec4(g_weather_color, menu_state::worldWeather.color);
		to_imvec4(g_crosshair_color, menu_state::worldScene.camCrosshairColor);

		apply_menu_accent();
	}

	void copy_to_buffer(const std::string& input, char* output, const size_t output_size)
	{
		if (!output || output_size == 0)
			return;

		std::snprintf(output, output_size, "%s", input.c_str());
	}

	void refresh_text_cache_from_settings()
	{
		g_player_name = menu_state::playerName;
		g_custom_clantag = menu_state::customClantag;

		for (int i = 0; i < 16; ++i)
		{
			g_chat_messages[i] = menu_state::chatMessages[i];
			g_kill_messages[i] = menu_state::killMessages[i];
		}

		g_text_cache_ready = true;
	}

	void push_text_cache_to_settings()
	{
		if (!g_text_cache_ready)
			refresh_text_cache_from_settings();

		copy_to_buffer(g_player_name, menu_state::playerName, sizeof(menu_state::playerName));
		copy_to_buffer(g_custom_clantag, menu_state::customClantag, sizeof(menu_state::customClantag));

		for (int i = 0; i < 16; ++i)
		{
			copy_to_buffer(g_chat_messages[i], menu_state::chatMessages[i], sizeof(menu_state::chatMessages[i]));
			copy_to_buffer(g_kill_messages[i], menu_state::killMessages[i], sizeof(menu_state::killMessages[i]));
		}
	}

	void push_player_name_to_settings()
	{
		if (!g_text_cache_ready)
			refresh_text_cache_from_settings();

		copy_to_buffer(g_player_name, menu_state::playerName, sizeof(menu_state::playerName));
	}

	void push_custom_clantag_to_settings()
	{
		if (!g_text_cache_ready)
			refresh_text_cache_from_settings();

		copy_to_buffer(g_custom_clantag, menu_state::customClantag, sizeof(menu_state::customClantag));
	}

	void push_message_cache_to_settings()
	{
		if (!g_text_cache_ready)
			refresh_text_cache_from_settings();

		for (int i = 0; i < 16; ++i)
		{
			copy_to_buffer(g_chat_messages[i], menu_state::chatMessages[i], sizeof(menu_state::chatMessages[i]));
			copy_to_buffer(g_kill_messages[i], menu_state::killMessages[i], sizeof(menu_state::killMessages[i]));
		}
	}

	std::vector<std::string> build_clantag_presets()
	{
		return {
			"Aimware", "Airflow", "BosniaHook", "Ev0lve", "Fatality", "GameSense",
			"Iniuria", "Legendware", "Millionware", "MonkeySquad", "Monolith",
			"Nemesis", "Neverlose", "Nixware", "Onetap.su", "Primordial",
			"RaweTrip", "Rifk", "Skeet.cc", "SpirtHack", "Weave", "ShanDongGanZhi",
			"Custom"
		};
	}

	std::vector<std::string> build_animation_styles()
	{
		return {
			"Rotate Left", "Rotate Right", "Progressive", "Retract", "Front Retract",
			"Scroll", "Time Percent", "Decode", "Typewriter", "Glitch", "Core Dump",
			"Penetrate", "Password Lock", "Scanline", "Heart", "CMD Spinner",
			"CMD Log", "CMD Dots", "Network Error", "Directory Brute Force"
		};
	}

	void refresh_config_items()
	{
		GetSettingsJson()->UpdateConfigList();
		g_config_items = GetSettingsJson()->GetConfigList();

		if (g_config_items.empty())
			g_config_items = { "Empty" };

		g_config_index = std::clamp(g_config_index, 0, static_cast<int>(g_config_items.size()) - 1);
	}

	std::string normalize_config_name(std::string name)
	{
		if (name.empty() && !g_config_items.empty() && g_config_items[g_config_index] != "Empty")
			name = g_config_items[g_config_index];

		if (name.empty())
			return {};

		if (name.size() < 5 || name.substr(name.size() - 5) != ".json")
			name += ".json";

		return name;
	}

	void clamp_menu_values()
	{
		const auto preset_count = static_cast<int>(build_clantag_presets().size());
		const auto anim_count = static_cast<int>(build_animation_styles().size());

		menu_state::clantagSelection = std::clamp(menu_state::clantagSelection, 0, std::max(0, preset_count - 1));
		menu_state::animationSelection = std::clamp(menu_state::animationSelection, 0, std::max(0, anim_count - 1));
		menu_state::chatCount = std::clamp(menu_state::chatCount, 1, 16);
		menu_state::killCount = std::clamp(menu_state::killCount, 1, 16);
		menu_state::fakeCooldownValue = std::clamp(menu_state::fakeCooldownValue, 0, 19);
		menu_state::fakeCooldownTime = std::clamp(menu_state::fakeCooldownTime, 0, 7);
		menu_state::fakeCooldownCustomDays = std::clamp(menu_state::fakeCooldownCustomDays, 1, 9999);
		vars::menuKey = std::clamp(vars::menuKey, 1, 255);
		if (vars::menuKey == VK_LBUTTON || vars::menuKey == VK_RBUTTON || vars::menuKey == VK_MBUTTON
			|| vars::menuKey == VK_XBUTTON1 || vars::menuKey == VK_XBUTTON2)
			vars::menuKey = VK_HOME;
	}

	void refresh_menu_key_from_settings()
	{
		vars::menuKey = std::clamp(vars::menuKey, 1, 255);
		g_menu_key.key = vars::menuKey;
		g_menu_key.mode = framework::key_mode_t::toggle;
	}

	void push_menu_key_to_settings()
	{
		if (g_menu_key.key > 0 && g_menu_key.key <= 255)
			vars::menuKey = g_menu_key.key;

		vars::menuKey = std::clamp(vars::menuKey, 1, 255);
		g_menu_key.key = vars::menuKey;
		g_menu_key.mode = framework::key_mode_t::toggle;
	}

	bool keybind_is_bound(const framework::key_var_t& keybind)
	{
		return keybind.key > 0 && keybind.key <= 255;
	}

	framework::widget_mode keybind_widget_mode(const framework::key_var_t& keybind)
	{
		if (keybind.mode == framework::key_mode_t::always)
			return framework::widget_mode::always;

		if (keybind.mode == framework::key_mode_t::toggle)
			return framework::widget_mode::toggle;

		return framework::widget_mode::hold;
	}

	void add_active_keybind(std::vector<framework::keybind_entry_t>& entries, const std::string& label, framework::key_var_t& keybind)
	{
		if (!keybind_is_bound(keybind) || !keybind.active())
			return;

		entries.push_back(framework::keybind_entry_t(label, keybind_widget_mode(keybind)));
	}

	void add_menu_keybind(std::vector<framework::keybind_entry_t>& entries)
	{
		if (!keybind_is_bound(g_menu_key) || !framework::g_ctx->m_open)
			return;

		entries.push_back(framework::keybind_entry_t("Menu", framework::widget_mode::toggle));
	}
}

namespace framework
{
	void c_menu::initialize()
	{
	slog::log::info("[>] started menu initialization");

		this->m_windows.clear();
		g_ctx->m_tabs.clear();

		refresh_color_cache_from_settings();
		refresh_text_cache_from_settings();
		refresh_config_items();
		refresh_menu_key_from_settings();

		auto window = std::make_shared<c_window>("PluginSense", math::c_vector_2d((core::g_overlay->width * 0.5f) - 350.f, (core::g_overlay->height * 0.5f) - 300.f), math::c_vector_2d(700.f, 600.f));
		{
			if (window == nullptr)
			{
				slog::log::error("[-] failed to create window");
			}
			else
			{
				window->prebuild_tabs([](framework::c_tab* controller) {
					controller->create_tab(ICON_FA_VISUAL, "Visual", { });
					controller->create_tab(ICON_FA_MODEL, "Changer", { });
					controller->create_tab(ICON_FA_COMMENT, "Chat", { });
					controller->create_tab(ICON_FA_BOMB, "Utility", { });
					controller->create_tab(ICON_FA_FOLDER, "Config", { });
					});
				window->finish_tab_prebuild();

				window->build_child("World", framework::child_width::half, half_height, [](framework::c_child* controller) {
					controller->attach_child("Visual", "", 0);

					// ---- Weather ----
					controller->add_checkbox("Weather", &menu_state::worldWeather.enabled);
					auto weather_settings = controller->add_popup("Weather", true, [](framework::c_popup* popup) {
						popup->add_dropdown("Weather type", &menu_state::worldWeather.type, { "Snow", "Rain", "Stars" });
						popup->add_colorpicker("Weather color", &g_weather_color);
					});
					weather_settings->set_inlined();

					// ---- Wind ----
					controller->add_checkbox("Wind", &menu_state::worldWeather.wind);
					auto wind_settings = controller->add_popup("Wind", true, [](framework::c_popup* popup) {
						popup->add_slider_float("Wind strength", &menu_state::worldWeather.windStrength, 0.f, 10.f);
						popup->add_slider_float("Wind direction", &menu_state::worldWeather.windDirection, 0.f, 360.f, false, L"\u00B0");
						popup->add_slider_float("Wind turbulence", &menu_state::worldWeather.windTurbulence, 0.f, 5.f);
					});
					wind_settings->set_inlined();

				// ---- Wetness ----
					controller->add_checkbox("Wetness", &menu_state::worldWeather.wetness);
					auto wetness_settings = controller->add_popup("Wetness", true, [](framework::c_popup* popup) {
						popup->add_slider_float("Wetness density", &menu_state::worldWeather.wetnessDensity, 0.f, 5.f);
						popup->add_slider_float("Wetness speed", &menu_state::worldWeather.wetnessSpeed, 0.1f, 3.f, false, L"x");
					});
					wetness_settings->set_inlined();

					// ---- Bloom ----
					controller->add_checkbox("Bloom", &menu_state::worldScene.bloom);
					auto bloom_settings = controller->add_popup("Bloom", true, [](framework::c_popup* popup) {
						popup->add_slider_float("Bloom value", &menu_state::worldScene.bloomValue, 0.f, 3.f);
					});
					bloom_settings->set_inlined();

					// ---- Gamma ----
					controller->add_checkbox("Gamma", &menu_state::worldScene.gamma);
					auto gamma_settings = controller->add_popup("Gamma", true, [](framework::c_popup* popup) {
						popup->add_slider_float("Gamma value", &menu_state::worldScene.gammaValue, 0.5f, 5.f);
					});
					gamma_settings->set_inlined();

					// ---- Depth of Field ----
					controller->add_checkbox("Depth of field", &menu_state::worldScene.dof);
				auto dof_settings = controller->add_popup("Depth of field", true, [](framework::c_popup* popup) {
				popup->add_checkbox("Aim focus", &menu_state::worldScene.dofFocus);
						popup->add_slider_float("Near blurry", &menu_state::worldScene.dofNearBlurry, 0.f, 50.f, false, L"u");
						popup->add_slider_float("Near crisp", &menu_state::worldScene.dofNearCrisp, 0.f, 100.f, false, L"u");
						popup->add_slider_float("Far crisp", &menu_state::worldScene.dofFarCrisp, 100.f, 2000.f, false, L"u");
						popup->add_slider_float("Far blurry", &menu_state::worldScene.dofFarBlurry, 200.f, 5000.f, false, L"u");
					});
					dof_settings->set_inlined();
				});

				window->build_child("Velocity", framework::child_width::half, half_height, [](framework::c_child* controller) {
					controller->attach_child("Visual", "", 0);

					controller->add_slider_float("Offset", &menu_state::velocityOffset, 30.f, 250.f, false, L"px");
					controller->add_checkbox("Velocity text", &menu_state::velocityText);
					auto high_speed = controller->add_colorpicker("High speed", &g_velocity_high);
					high_speed->set_inlined();
					auto mid_speed = controller->add_colorpicker("Mid speed", &g_velocity_mid);
					mid_speed->set_inlined();
					auto low_speed = controller->add_colorpicker("Low speed", &g_velocity_low);
					low_speed->set_inlined();
					controller->add_checkbox("Velocity graph", &menu_state::velocityGraph);
					auto graph_color = controller->add_colorpicker("Graph color", &g_velocity_graph);
					graph_color->set_inlined();
					controller->add_checkbox("Keystrokes", &menu_state::keystrokes);
				});

				window->build_child("Weapon Model", framework::child_width::half, full_height, [](framework::c_child* controller) {
					controller->attach_child("Changer", "", 1);

					controller->add_checkbox("Enabled", &menu_state::weaponModelEnabled);

					const auto weaponNames = GetWeaponModel()->GetWeaponNames();
					menu_state::weaponModelWeapon = std::clamp(menu_state::weaponModelWeapon, 0, static_cast<int>(weaponNames.size()) - 1);

					controller->add_dropdown("Weapon", &menu_state::weaponModelWeapon, weaponNames)
						->icon_stack([] { return GetWeaponModel()->GetWeaponIconChars(); });

					controller->add_listbox("Model", &menu_state::weaponModelModel, {}, 160.f)
						->execute_stack([] {
							// 每次渲染:把当前武器已选的模型索引载入
							GetWeaponModel()->SyncWeaponToModel(menu_state::weaponModelWeapon, menu_state::weaponModelModel);
							return GetWeaponModel()->GetModelNames();
						})
						->on_select([](int) {
							// 选中模型后写回当前武器
							GetWeaponModel()->SyncModelToWeapon(menu_state::weaponModelWeapon, menu_state::weaponModelModel);
						});

					// 重新扫描 csgo\weapons,读取新添加的模型
					controller->add_button("Refresh", [] { GetWeaponModel()->Refresh(); });
				});

				window->build_child("Name Changer", framework::child_width::half, full_height, [](framework::c_child* controller) {
					controller->attach_child("Changer", "", 1);

					controller->add_checkbox("Enabled", &menu_state::clantagEnabled);
					controller->add_dropdown("Preset", &menu_state::clantagSelection, build_clantag_presets())
						->execute_stack([] { return build_clantag_presets(); });
					controller->add_checkbox("Animate clantag", &menu_state::animatedClantag)->set_callback_visibility([] { return menu_state::clantagSelection == 22; });
					auto animation_settings = controller->add_popup("Animation", true, [](framework::c_popup* popup) {
						popup->add_dropdown("Animation style", &menu_state::animationSelection, build_animation_styles())
							->execute_stack([] { return build_animation_styles(); });
					});
					animation_settings->set_inlined();
					animation_settings->set_callback_visibility([] { return menu_state::clantagSelection == 22; });
					controller->add_slider_float("Animation speed", &menu_state::clantagSpeed, 0.1f, 1.f, false, L"s")
						->set_callback_visibility([] { return menu_state::clantagSelection != 22 || menu_state::animatedClantag; });
					controller->add_input_box("Clantag", &g_custom_clantag)->set_callback_visibility([] { return menu_state::clantagSelection == 22; });
					controller->add_button("Apply clantag", [] {
						push_custom_clantag_to_settings();
						GetNameChanger()->ApplyName();
					})->set_callback_visibility([] { return menu_state::clantagSelection == 22; });
					controller->add_input_box("Name", &g_player_name);
					controller->add_button("Apply name", [] {
						push_player_name_to_settings();
						GetNameChanger()->ApplyName();
					});
				});

				window->build_child("Other", framework::child_width::half, full_height, [](framework::c_child* controller) {
					controller->attach_child("Visual", "", 0);

					controller->add_checkbox("Fortnite damage", &menu_state::damageIndicator);
					auto damage_settings = controller->add_popup("Fortnite damage", true, [](framework::c_popup* popup) {
						popup->add_colorpicker("Headshot color", &g_damage_head);
						popup->add_colorpicker("Damage color", &g_damage_body);
						popup->add_slider_float("Damage scale", &menu_state::damageScale, 0.25f, 2.f);
						popup->add_slider_float("Damage time", &menu_state::damageTime, 1.f, 10.f, false, L"s");
					});
					damage_settings->set_inlined();
			controller->add_checkbox("Motion Camera", &menu_state::worldScene.motionCamera);
					auto cam_settings = controller->add_popup("Motion Camera", true, [](framework::c_popup* popup) {
						popup->add_slider_float("Hor offset", &menu_state::worldScene.camHorOffset, -30.f, 30.f);
						popup->add_slider_float("Ver offset", &menu_state::worldScene.camVerOffset, -50.f, 50.f);
				popup->add_slider_float("Smoothing", &menu_state::worldScene.camSlack, 1.f, 100.f);
					popup->add_checkbox("Crosshair", &menu_state::worldScene.camCrosshair);
				auto cr_col = popup->add_colorpicker("Crosshair color", &g_crosshair_color);
					cr_col->set_inlined();
						auto cr_len = popup->add_slider_float("Line length", &menu_state::worldScene.camCrosshairLength, 4.f, 20.f, false, L"px");
						cr_len->set_callback_visibility([] { return menu_state::worldScene.camCrosshair; });
						auto cr_thk = popup->add_slider_float("Thickness", &menu_state::worldScene.camCrosshairThickness, 1.f, 4.f, false, L"px");
						cr_thk->set_callback_visibility([] { return menu_state::worldScene.camCrosshair; });
					});
					cam_settings->set_inlined();
						controller->add_checkbox("Spoof", &menu_state::spoof);
						auto spoof_settings = controller->add_popup("Spoof", true, [](framework::c_popup* popup) {
							// ③ 其他
							popup->add_checkbox("VacNet Reveal", &menu_state::vacnetEnabled);

							// ① 惩罚 / 状态(互斥)
							popup->add_checkbox("Official Ban", &menu_state::officialBan)->on_change([](bool v) {
								if (v) { menu_state::fakeCooldown = false; menu_state::vacBan = false; }
							});
							popup->add_checkbox("VAC Ban", &menu_state::vacBan)->on_change([](bool v) {
								if (v) { menu_state::fakeCooldown = false; menu_state::officialBan = false; }
							});
							popup->add_checkbox("Cooldown", &menu_state::fakeCooldown)->on_change([](bool v) {
								if (v) { menu_state::officialBan = false; menu_state::vacBan = false; }
							});
							auto cooldown_type = popup->add_dropdown("Cooldown type", &menu_state::fakeCooldownValue, {
								// 作弊 / 封禁
								"Convicted Behavior", "Convicted Cheating", "GSLT Violation", "VacNet Culprit", "VacNet Affiliate",
								// 行为 / 举报
								"Griefing",
								// 比赛
								"Abandon", "Abandon Grace", "Disconnected", "Disconnect Grace", "Failed Connect",
								"Kicked", "Kicked Too Much", "Kick Abuse",
								// 伤害 / TK
								"TK Limit", "TK Spawn", "TH Limit", "TH Spawn",
								// 其他
								"Skill Calibration", "Unknown"
							});
							cooldown_type->set_callback_visibility([] { return menu_state::fakeCooldown; });
							auto cooldown_time = popup->add_dropdown("Cooldown time", &menu_state::fakeCooldownTime, { "30 Mins", "20 Hours", "7 Days", "30 Days", "181 Days", "365 Days", "3650 Days", "Custom" });
							cooldown_time->set_callback_visibility([] { return menu_state::fakeCooldown; });
							auto custom_days = popup->add_slider_int("Custom days", &menu_state::fakeCooldownCustomDays, 1, 9999, false, L"d");
							custom_days->set_callback_visibility([] { return menu_state::fakeCooldown && menu_state::fakeCooldownTime == 7; });

							// ② 账号资料
							popup->add_checkbox("Prime", &menu_state::lobbyPrime);
							popup->add_checkbox("Premier", &menu_state::lobbyPremier);
							auto premier_visible = [] { return menu_state::lobbyPremier; };
							auto premier_rating = popup->add_slider_int("CS Rating", &menu_state::lobbyPremierRating, 0, 99999, false, L"");
							premier_rating->set_callback_visibility(premier_visible);
							auto premier_wins = popup->add_slider_int("Wins", &menu_state::lobbyPremierWins, 0, 9999, false, L"");
							premier_wins->set_callback_visibility(premier_visible);
							popup->add_checkbox("Wingman", &menu_state::lobbyWingman);
							auto wingman_visible = [] { return menu_state::lobbyWingman; };
							auto wingman_rating = popup->add_slider_int("Rank Level", &menu_state::lobbyWingmanRating, 0, 18, false, L"");
							wingman_rating->set_callback_visibility(wingman_visible);
							auto wingman_wins = popup->add_slider_int("Wins", &menu_state::lobbyWingmanWins, 0, 9999, false, L"");
							wingman_wins->set_callback_visibility(wingman_visible);
							popup->add_checkbox("Level", &menu_state::lobbyLevel);
							auto level_visible = [] { return menu_state::lobbyLevel; };
							auto level_value = popup->add_slider_int("Level", &menu_state::lobbyLevelValue, 0, 40, false, L"");
							level_value->set_callback_visibility(level_visible);
							auto level_xp = popup->add_slider_int("XP", &menu_state::lobbyXp, 0, 5000, false, L"");
							level_xp->set_callback_visibility(level_visible);
						});
						spoof_settings->set_inlined();
					controller->add_checkbox("Damage Log", &menu_state::damageLogEnabled);
					auto log_settings = controller->add_popup("Damage Log", true, [](framework::c_popup* popup) {
						popup->add_multibox("Events", false, [](framework::c_multidropdown* box) {
							box->add_selection("Hit", &menu_state::hitlogEnabled);
							box->add_selection("Hurt", &menu_state::hitlogVictim);
						});
						popup->add_dropdown("Log style", &menu_state::hitlogType, { "PluginSense", "MemeSense" });
					});
					log_settings->set_inlined();
				});

				window->build_child("Chat spammer", framework::child_width::half, full_height, [](framework::c_child* controller) {
					controller->attach_child("Chat", "", 2);

					controller->add_checkbox("Enable chat spammer", &menu_state::chatSpammer);
					controller->add_button("Add message", [] {
						if (menu_state::chatCount < 16)
							++menu_state::chatCount;
					});
					controller->add_button("Remove message", [] {
						if (menu_state::chatCount > 1)
						{
							--menu_state::chatCount;
							g_chat_messages[menu_state::chatCount].clear();
						}
					})->set_inlined();
					controller->add_slider_float("Send delay", &menu_state::sendDelay, 0.1f, 20.f, false, L"s");

					for (int i = 0; i < 16; ++i)
					{
						auto input = controller->add_input_box(i == 0 ? "Message text" : "Message text " + std::to_string(i), &g_chat_messages[i]);
						input->set_callback_visibility([i] { return i < menu_state::chatCount; });
					}
				});

				window->build_child("Kill say", framework::child_width::half, full_height, [](framework::c_child* controller) {
					controller->attach_child("Chat", "", 2);

					controller->add_checkbox("Enable kill say", &menu_state::killSay);
					controller->add_button("Add kill say", [] {
						if (menu_state::killCount < 16)
							++menu_state::killCount;
					});
					controller->add_button("Remove kill say", [] {
						if (menu_state::killCount > 1)
						{
							--menu_state::killCount;
							g_kill_messages[menu_state::killCount].clear();
						}
					})->set_inlined();

					for (int i = 0; i < 16; ++i)
					{
						auto input = controller->add_input_box(i == 0 ? "Message text" : "Message text " + std::to_string(i), &g_kill_messages[i]);
						input->set_callback_visibility([i] { return i < menu_state::killCount; });
					}
				});

				window->build_child("Settings", framework::child_width::half, full_height, [](framework::c_child* controller) {
					controller->attach_child("Config", "", 4);

					controller->add_listbox("Config list", &g_config_index, g_config_items, 180)
						->execute_stack([] { return g_config_items; });
					controller->add_input_box("Config name", &g_config_name);

					controller->add_button("Save config", []() {
						push_color_cache_to_settings();
						push_message_cache_to_settings();

						const auto name = normalize_config_name(g_config_name);
						if (!name.empty())
						{
							GetSettingsJson()->SaveConfig(name);
							refresh_config_items();
							slog::log::success("saved config {}", name);
						}
					});

					controller->add_button("Load config", []() {
						if (!g_config_items.empty() && g_config_items[g_config_index] != "Empty")
						{
							const auto name = g_config_items[g_config_index];
							GetSettingsJson()->LoadConfig(name);
							refresh_color_cache_from_settings();
							refresh_text_cache_from_settings();
							refresh_menu_key_from_settings();
							slog::log::success("loaded config {}", name);
						}
					})->set_inlined();

					controller->add_button("Refresh list", []() {
						refresh_config_items();
					});
					controller->add_button("Delete config", []() {
						if (!g_config_items.empty() && g_config_items[g_config_index] != "Empty")
						{
							const auto name = g_config_items[g_config_index];
							GetSettingsJson()->DeleteConfig(name);
							refresh_config_items();
							slog::log::success("deleted config {}", name);
						}
					})->set_inlined();
				});

				window->build_child("Menu", framework::child_width::half, full_height, [](framework::c_child* controller) {
					controller->attach_child("Config", "", 4);

					controller->add_keybind("Menu Key", &g_menu_key)->key_only()->keyboard_only()->suppress_next_keyup();
					controller->add_colorpicker("Menu color", &g_menu_accent);
					controller->add_checkbox("Keybinds", &vars::menuKeybinds);
				});

				window->build_child("Helper Beta", framework::child_width::half, full_height, [](framework::c_child* controller) {
					controller->attach_child("Utility", "", 3);

					controller->add_checkbox("Enabled", &menu_state::helperEnabled);
					controller->add_keybind("Helper key", &helper::g_helper_key)->key_only()->suppress_next_keyup();
					controller->add_checkbox("Aim assist", &menu_state::autoAim);
					controller->add_checkbox("Auto release", &menu_state::autoExecute);
					controller->add_slider_int("Aim smoothing", &menu_state::aimSpeed, 1, 30);
					controller->add_slider_float("Aim threshold", &menu_state::aimThreshold, 0.05f, 3.f, false, L"\u00B0");
					controller->add_slider_int("Lock time", &menu_state::lockTimeMs, 0, 250, false, L"ms");
					controller->add_slider_int("Draw distance", &menu_state::drawDistance, 100, 2000);
					controller->add_slider_int("Stand distance", &menu_state::standDistance, 50, 600);
					controller->add_slider_float("Stand radius", &menu_state::standRadius, 1.f, 100.f, false, L"u");
					controller->add_slider_float("Release radius", &menu_state::releaseRadius, 1.f, 20.f, false, L"u");
					controller->add_slider_float("Height tolerance", &menu_state::heightTolerance, 1.f, 32.f, false, L"u");
					controller->add_checkbox("Show action", &menu_state::showAction);
					controller->add_checkbox("Show distance", &menu_state::showDistance);
				});

				window->build_child("Keys", framework::child_width::half, full_height, [](framework::c_child* controller) {
					controller->attach_child("Utility", "", 3);

					controller->add_keybind("Forward key", &helper::g_move_forward)->key_only();
					controller->add_keybind("Backward key", &helper::g_move_back)->key_only();
					controller->add_keybind("Left key", &helper::g_move_left)->key_only();
					controller->add_keybind("Right key", &helper::g_move_right)->key_only();
					controller->add_keybind("Walk key", &helper::g_move_walk)->key_only();
					controller->add_keybind("Crouch key", &helper::g_move_duck)->key_only();
					controller->add_keybind("Jump key", &helper::g_move_jump)->key_only();
					controller->add_keybind("Throw key", &helper::g_attack_key)->key_only();
					controller->add_keybind("Alt throw key", &helper::g_attack2_key)->key_only();
				});

				this->m_windows.push_back(window);
			}
		}

		auto widget = std::make_shared<c_widgets>();
		{
			if (widget == nullptr)
			{
				slog::log::error("[-] failed to create widget");
			}
			else
			{
				auto final2 = g_widget_ctx.m_keybind_pos + math::c_vector_2d(0.f, core::g_overlay->height * 0.5f);

				widget->create_widget(widget_type::keybind, final2);
				this->m_widgets = widget;
			}
		}
	}

	void c_menu::runtime()
	{
		g_ctx->m_click_consumed = false;

		clamp_menu_values();

		for (auto& windows : this->m_windows)
		{
			windows->input();
			windows->paint();
		}

		push_color_cache_to_settings();
		push_message_cache_to_settings();
		push_menu_key_to_settings();


		render_widgets();

		g_search.paint_database();
	}

	void c_menu::render_widgets()
	{
		if (!this->m_widgets || !this->m_widgets->keybind_manager())
			return;

		std::vector<framework::keybind_entry_t> entries;
		add_menu_keybind(entries);
		add_active_keybind(entries, "Helper", helper::g_helper_key);
		this->m_widgets->keybind_manager()->update_keybinds(entries);
		this->m_widgets->draw();
	}

	std::shared_ptr<c_notify_panel> c_menu::notify()
	{
		return this->m_widgets->notify();
	}
}

// ============================================================================
// helper 热键(外部 helper 与 UI 共享)
// 按键绑定默认对齐 vesta/CS2 默认:W / Shift / Ctrl / Space / 左键 / 右键
// ============================================================================
namespace helper
{
	framework::key_var_t g_helper_key{}; // 默认不绑键,用户在 Keybinds 分区绑定
	framework::key_var_t g_move_forward{ 'W' , framework::key_mode_t::hold };
	framework::key_var_t g_move_back{ 'S' , framework::key_mode_t::hold };
	framework::key_var_t g_move_left{ 'A' , framework::key_mode_t::hold };
	framework::key_var_t g_move_right{ 'D' , framework::key_mode_t::hold };
	framework::key_var_t g_move_walk{ VK_SHIFT , framework::key_mode_t::hold };
	framework::key_var_t g_move_duck{ VK_CONTROL , framework::key_mode_t::hold };
	framework::key_var_t g_move_jump{ VK_SPACE , framework::key_mode_t::hold };
	framework::key_var_t g_attack_key{ VK_LBUTTON , framework::key_mode_t::hold };
	framework::key_var_t g_attack2_key{ VK_RBUTTON , framework::key_mode_t::hold };
}
