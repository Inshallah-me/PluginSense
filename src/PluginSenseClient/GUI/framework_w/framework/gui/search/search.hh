#pragma once

namespace framework
{
	struct search_database_t
	{
		std::shared_ptr<c_base_element> m_copied_controls{};
		float m_backup_x{}; // restore control


		std::string tab_name{};
		int tab_index{}; // this prob shoudnt be a pointer, who cares
	};

	class c_search
	{
	public:
		void debug_database();
		void paint_database();

		void set_pos(math::c_vector_2d pos) { this->m_pos = pos; }

		// this wont need a constructor, not yet atleast
		void add_to_database(std::shared_ptr<c_base_element> control, std::string tab_name, int tab_index);

		void cleanup();

		bool m_should_draw{};
		bool m_focused{};
		utils::anim_context_t m_search_anim{};

		math::c_vector_2d m_backup_size{};
		std::vector< search_database_t > m_database{};
	private:

		utils::anim_context_t m_search_focusing{};
		std::string m_search_text{};

		struct ghost_char_t {
			char  m_glyph;
			float m_anim;   
			float m_x;      
		};

		std::vector<ghost_char_t> m_ghost_chars;

		std::vector<float> m_char_anims;  
		std::vector<bool>  m_char_removing;

		float blink{};

		math::c_vector_2d m_pos{};
	};

	// no need of shared shit
	inline c_search g_search;
}