#pragma once

namespace framework
{
	struct star_t {
		math::c_vector_2d m_pos;        // real position
		math::c_vector_2d m_render_pos; // smoothed position used for drawing
		math::c_vector_2d m_vel;
		float m_anim;
		float m_speed;
		float m_size;
		const char* m_glyph;
	};

	class c_window
	{
	public:
		c_window(std::string title, math::c_vector_2d pos, math::c_vector_2d size);
		~c_window() // no point in creating this in cc file as its only a destructor
		{
			slog::log::success("[+] window {} destroyed", m_title);
		}

		// no virtual as this is the base object
		void paint();
		void setup_objects();
		void input();

		std::shared_ptr<c_child> build_child(std::string name, child_width width, float y, std::function<void(c_child* ptr)> callback);
		std::shared_ptr<c_tab> prebuild_tabs(std::function<void(c_tab* ptr)> callback);

		void finish_tab_prebuild();

		// child pading func
		math::c_vector_2d subtab_padding();
		math::c_vector_2d child_padding();
	private:
		std::string m_prev_tab{};
		std::string m_prev_subtab{};
		bool m_tab_switching{ false };

		// tab pointer
		std::shared_ptr<c_tab> m_obj_tab{};

		std::string m_title{};
		math::c_vector_2d m_pos{};
		math::c_vector_2d m_size{};

		// window animation
		utils::anim_context_t m_window_opacity{};
		math::c_vector_2d m_last_window_pos{};

		// copy it
		std::shared_ptr<c_interactive_preview> m_preview{};

		std::vector<star_t> m_stars;
		float m_star_spawn_timer = 0.f;

		// childrens
		std::vector<std::shared_ptr<c_child>> m_childrens{};
	};

}