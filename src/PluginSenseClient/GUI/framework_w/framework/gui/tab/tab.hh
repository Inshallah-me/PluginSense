#pragma once

namespace framework
{
	class c_tab
	{
	public:
		c_tab(math::c_vector_2d pos, math::c_vector_2d size, utils::anim_context_t parent);

		void paint();
		void update_input(math::c_vector_2d pos, math::c_vector_2d size);

		void create_tab(std::string icon, std::string name, std::vector<std::string> subtabs);

		utils::anim_context_t parent_opcity{};
	private:
		math::c_vector_2d m_pos{}, m_size{};

		std::string m_tab{}, m_icon{};
		std::function<void()> m_callback{};
	};
}