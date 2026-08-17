#pragma once
#include <fstream>
#include <sstream>

namespace core
{
	// create configuration base
	struct config_base_t {
		virtual void save(std::ostream& os) = 0;
		virtual void load(const std::string& value) = 0;

		// deconstructor
		virtual ~config_base_t() = default;
	};

	template<typename T>
	struct config_entry : config_base_t {
		T* ptr;

		config_entry(T* p) : ptr(p) {}

		void save(std::ostream& os) override {
			os << *ptr;
		}

		void load(const std::string& str) override {
			std::istringstream ss(str);
			ss >> *ptr;
		}
	};

	template<>
	struct config_entry<math::c_vector_2d> : config_base_t {
		math::c_vector_2d* member; // i shouldnt need to make this a ptr

		config_entry(math::c_vector_2d* p) : member(p) {}

		void save(std::ostream& os) override {
			os << member->x << ","
				<< member->y;
		}

		void load(const std::string& str) override {
			sscanf_s(
				str.c_str(),
				"%f,%f",
				&member->x,
				&member->y
			);
		}
	};

	template<>
	struct config_entry<bool[5]> : config_base_t {
		bool (*ptr)[5]; // pointer to array of 4 floats

		config_entry(bool (*p)[5]) : ptr(p) {}
		void save(std::ostream& os) override {
			os << (*ptr)[0] << "," << (*ptr)[1] << "," << (*ptr)[2] << "," << (*ptr)[3] << "," << (*ptr)[4];
		}

		void load(const std::string& str) override {
			int a, b, c, d, e;
			sscanf_s(str.c_str(), "%d,%d,%d,%d,%d", &a, &b, &c, &d, &e);
			(*ptr)[0] = a != 0;
			(*ptr)[1] = b != 0;
			(*ptr)[2] = c != 0;
			(*ptr)[3] = d != 0;
			(*ptr)[4] = e != 0;
		}
	};

	template<>
	struct config_entry<hue::c_color> : config_base_t {
		hue::c_color* ptr;

		config_entry(hue::c_color* p) : ptr(p) {}

		void save(std::ostream& os) override {
			os << ptr->r << ","
				<< ptr->g << ","
				<< ptr->b << ","
				<< ptr->a << ",";
		}

		void load(const std::string& str) override {
			sscanf_s(
				str.c_str(),
				"%d,%d,%d,%d",
				&ptr->r,
				&ptr->g,
				&ptr->b,
				&ptr->a
			);
		}
	};

	template<>
	struct config_entry<framework::key_var_t> : config_base_t {
		framework::key_var_t* ptr;

		config_entry(framework::key_var_t* p) : ptr(p) {}

		void save(std::ostream& os) override {
			os << ptr->key << "," << ptr->mode;
		}

		void load(const std::string& str) override {
			sscanf_s(str.c_str(), "%d,%d", &ptr->key, &ptr->mode);
			ptr->reset_runtime_state();
		}
	};

	struct crosshair_config_t {
		bool enabled{ false };
		int style{ 0 };          // 0=classic, 1=circle, 2=cross+circle
		float size{ 5.f };       // arm length
		float thickness{ 1.f };  // line thickness
		float gap{ 3.f };        // center gap
		bool dot{ false };       // center dot
		float dot_size{ 1.f };   // center dot radius
		bool outline{ true };    // black outline around lines
		float outline_thickness{ 1.f };
		bool t_shape{ false };   // remove top arm
		hue::c_color color{ 0, 255, 0, 255 };
		hue::c_color outline_color{ 0, 0, 0, 255 };
	};

	class c_config_manager {
	public:
		std::unordered_map<std::string, std::unique_ptr<config_base_t>> fields;

		// macros
#define REGISTER_FIELD(name) \
		fields[#name] = std::make_unique<config_entry<decltype(name)>>(&name); \

		bool esp{ false }, name{ false }, box{ false }, box_shadow{false}, weapon_shadow{false}, health_shadow{false}, ammo_shadow{false}, ammo{false}, name_shadow{false}, esp_informations{false}, weapon{false}, health{false}, extend_boxing{false}, team_check_disable{false}, sound_esp{}, bypass_sound{}, limit_by_distance{}, gradient_skeleton{}, skeleton{}, box_gradient{}, weapon_ico{}, weapon_name{}, sonar_esp{}, aimbot{}, hitbox_optimization{};
		hue::c_color box_color{ }, box_color2{}, name_color{}, weapon_color{}, health_main_color{}, health_second_color{}, ammo_main_color{}, ammo_second_color{}, sound_color{}, skeleton_color{}, skeleton_color_2{};
		int box_type{}, skeleton_type{}, name_font{}, weapon_font, weapon_icon_flag{}, health_color_type{}, ammo_color_type{}, sound_shape{}, max_esp_distance{ 50 }, skeleton_gradient_start{ 10 }, sonar_volume{};
		bool flags[5]{ false }, aimbot_hitboxes[5]{ false };
		float sound_radius{ 5.f }, smoothing{ 10.f }, fov{};

		bool overlay_vsync{true};
		bool overlay_flushing{ true };

		hue::c_color box_color_inv{ }, box_color2_inv{}, name_color_inv{}, weapon_color_inv{},
			health_main_color_inv{}, health_second_color_inv{}, ammo_main_color_inv{}, ammo_second_color_inv{}, skeleton_color_inv{}, skeleton_color_2_inv{};

		bool hitlog{ false };
		bool damage_markers{ false };
		bool hitmarker{ false };
		bool hitmarker_3d{ false };
		bool bullet_tracer{ false };
		hue::c_color hitlog_color{ 255, 80, 80, 255 };
		hue::c_color damage_markers_color{ 255, 255, 255, 255 };
		hue::c_color hitmarker_color{ 255, 255, 255, 255 };
		hue::c_color bullet_tracer_color{ 255, 255, 255, 255 };
		hue::c_color hitmarker3d_color{ 255, 255, 255, 255 };

		ash_config_t ash_particles;
		dissolve_config_t dissolve;
		hue::c_color dissolve_color{ 255, 255, 255, 255 };

		bool customize_invisible_esp{ false };

		bool chams{ false };
		bool grenade_prediction{ false };
		bool chams_wallhack{ false };    
		bool override_invisible_material{false};
		int chams_type_inv{ 0 };
		bool chams_wireframe{ false };  
		int  chams_type{ 0 };           
		hue::c_color chams_color{ 255, 100, 100, 255 };
		hue::c_color chams_color_wh{ 255, 100, 100, 255 };  
		hue::c_color chams_color_wire{ 255, 255, 255, 255 };
		hue::c_color chams_color_rim{ 80, 140, 255, 255 };

		hue::c_color lightning_color{ 160, 80, 255, 255 };  // hit color
		hue::c_color lightning_kill_color{ 80, 180, 255, 255 }; // kill color

		hue::c_color chams_color_rim_ragdol{ 80, 140, 255, 255 };
		hue::c_color chams_color_rim_invisible{ 80, 140, 255, 255 };

		bool chams_wireframe_invisible{ false };
		bool chams_wireframe_ragdoll{ false };
		hue::c_color chams_color_wire_invisible{ 255, 255, 255, 255 };
		hue::c_color chams_color_wire_ragdol{ 255, 255, 255, 255 };

		crosshair_config_t crosshair;
		int glow_type{ 0 };              
		float glow_thickness{ 2.f };     
		hue::c_color glow_color{ 255, 180, 50, 200 }; 

		bool pixel_depth{};

		// still in beta
		framework::key_var_t esp_activation{};
		framework::key_var_t aimbot_activation{};

		bool ragdoll_chams{};
		bool override_material_ragdol{};
		int ragdol_material{};
		hue::c_color ragdoll_invisible{ 255, 180, 50, 200 };
		hue::c_color ragdoll_visible{ 255, 180, 50, 200 };

		bool dropped_weapons{ false };
		hue::c_color dropped_accent{ 255, 255, 255 };
		int dropepd_radius{30};

		bool night_mode{ false };
		hue::c_color night_modec{ 0,0,0, 10 };

		bool draw_molotov{ false }; hue::c_color drawn_molotov{ 250, 116, 32 };
		bool molotov_radius{ false }; hue::c_color drawn_radius{ 250, 116, 32 };

		bool draw_smoke{ false }; hue::c_color drawn_smoke{ 3, 202, 252 };
		bool smoke_radius{ false }; hue::c_color drawn_radius_smoke{ 3, 202, 252 };
		bool offscreen_esp = false;
		hue::c_color offscreen_color = hue::c_color(0, 184, 255, 255);

		// widgets
		math::c_vector_2d keybinds_pos{};

		// register fields
		void register_all() {
			REGISTER_FIELD(esp);

			REGISTER_FIELD(offscreen_esp);
			REGISTER_FIELD(offscreen_color);
			REGISTER_FIELD(draw_molotov);
			REGISTER_FIELD(drawn_molotov);
			REGISTER_FIELD(molotov_radius);
			REGISTER_FIELD(drawn_radius);

			REGISTER_FIELD(dropped_accent);
			REGISTER_FIELD(dropepd_radius);

			REGISTER_FIELD(dissolve.enabled);
			REGISTER_FIELD(dissolve.max_particles);
			REGISTER_FIELD(dissolve.spread_speed);
			REGISTER_FIELD(dissolve.size_min);
			REGISTER_FIELD(dissolve.glow_intensity);
			REGISTER_FIELD(dissolve.size_max);
			REGISTER_FIELD(dissolve.duration);
			REGISTER_FIELD(dissolve.glow_intensity);
			REGISTER_FIELD(dissolve.color);


			REGISTER_FIELD(ragdoll_chams);
			REGISTER_FIELD(ragdoll_invisible);
			REGISTER_FIELD(ragdoll_visible);
			REGISTER_FIELD(chams_type_inv);
			REGISTER_FIELD(override_invisible_material);
			REGISTER_FIELD(override_material_ragdol);
			REGISTER_FIELD(ragdol_material);
			REGISTER_FIELD(chams_color_rim_ragdol);
			REGISTER_FIELD(chams_color_rim_invisible);

			REGISTER_FIELD(hitmarker);
			REGISTER_FIELD(damage_markers);
			REGISTER_FIELD(damage_markers_color);
			REGISTER_FIELD(hitmarker_3d);
			REGISTER_FIELD(bullet_tracer);
			REGISTER_FIELD(hitmarker_color);
			REGISTER_FIELD(hitmarker3d_color);
			REGISTER_FIELD(box_color_inv);
			REGISTER_FIELD(overlay_vsync);
			REGISTER_FIELD(overlay_flushing);
			REGISTER_FIELD(box_color2_inv);
			REGISTER_FIELD(name_color_inv);
			REGISTER_FIELD(weapon_color_inv);
			REGISTER_FIELD(health_main_color_inv);
			REGISTER_FIELD(health_second_color_inv);
			REGISTER_FIELD(ammo_main_color_inv);
			REGISTER_FIELD(ammo_second_color_inv);
			REGISTER_FIELD(sound_color);
			REGISTER_FIELD(skeleton_color_inv);
			REGISTER_FIELD(skeleton_color_2_inv);
			REGISTER_FIELD(customize_invisible_esp);
			REGISTER_FIELD(box);
			REGISTER_FIELD(box_type);
			REGISTER_FIELD(skeleton_type);
			REGISTER_FIELD(extend_boxing);
			REGISTER_FIELD(crosshair.enabled);
			REGISTER_FIELD(crosshair.style);
			REGISTER_FIELD(crosshair.size);
			REGISTER_FIELD(glow_type);
			REGISTER_FIELD(glow_thickness);
			REGISTER_FIELD(glow_color);
			REGISTER_FIELD(grenade_prediction);
			REGISTER_FIELD(crosshair.thickness);
			REGISTER_FIELD(crosshair.gap);
			REGISTER_FIELD(crosshair.dot);
			REGISTER_FIELD(crosshair.dot_size);
			REGISTER_FIELD(crosshair.outline);
			REGISTER_FIELD(crosshair.t_shape);
			REGISTER_FIELD(crosshair.color);
			REGISTER_FIELD(team_check_disable);
			REGISTER_FIELD(box_color);
			REGISTER_FIELD(name_color);
			REGISTER_FIELD(hitlog);
			REGISTER_FIELD(hitlog_color);
			REGISTER_FIELD(chams);
			REGISTER_FIELD(chams_wallhack);
			REGISTER_FIELD(chams_color_rim);
			REGISTER_FIELD(chams_wireframe);
			REGISTER_FIELD(chams_type);
			REGISTER_FIELD(chams_color);
			REGISTER_FIELD(chams_color_wh);
			REGISTER_FIELD(chams_color_wire);
			REGISTER_FIELD(name);
			REGISTER_FIELD(name_font);
			REGISTER_FIELD(ammo);
			REGISTER_FIELD(weapon);
			REGISTER_FIELD(weapon_color);
			REGISTER_FIELD(weapon_font);
			REGISTER_FIELD(health);
			REGISTER_FIELD(health_main_color);
			REGISTER_FIELD(health_second_color);
			REGISTER_FIELD(health_color_type);
			REGISTER_FIELD(ammo_main_color);
			REGISTER_FIELD(ammo_second_color);
			REGISTER_FIELD(ammo_color_type);
			REGISTER_FIELD(flags);
			REGISTER_FIELD(esp_activation);
			REGISTER_FIELD(keybinds_pos);
			REGISTER_FIELD(sound_shape);
			REGISTER_FIELD(sound_color);
			REGISTER_FIELD(sound_esp);
			REGISTER_FIELD(sound_radius);
			REGISTER_FIELD(limit_by_distance);
			REGISTER_FIELD(bypass_sound);
			REGISTER_FIELD(max_esp_distance);
			REGISTER_FIELD(sonar_volume);
			REGISTER_FIELD(sonar_esp);
			REGISTER_FIELD(gradient_skeleton);
			REGISTER_FIELD(skeleton);
			REGISTER_FIELD(skeleton_color);
			REGISTER_FIELD(skeleton_color_2);
			REGISTER_FIELD(skeleton_gradient_start);
			REGISTER_FIELD(weapon_icon_flag);
			REGISTER_FIELD(weapon_name);
			REGISTER_FIELD(weapon_ico);
			REGISTER_FIELD(box_gradient);
			REGISTER_FIELD(box_color2);
			REGISTER_FIELD(aimbot);
			REGISTER_FIELD(aimbot_activation);
			REGISTER_FIELD(smoothing);
			REGISTER_FIELD(fov);
			REGISTER_FIELD(aimbot_hitboxes);
			REGISTER_FIELD(hitbox_optimization);
		}

		bool save(const std::string& config_name);
		bool load(const std::string& config_name);
		std::vector<std::string> get_all_configs(const std::string& folder = "C:\\phantosia\\");
	};

	inline c_config_manager g_config;
}
