#pragma once

namespace framework
{
	class c_listbox : public c_base_element
	{
	public:
		c_listbox(std::string label, int* var, std::vector<std::string> items, float height, bool hide_label = false);

		void draw() override;
		void input() override;

		c_listbox* execute_stack(std::function<std::vector<std::string>()> fn)
		{
			m_call_stacks = [this, fn = std::move(fn)]() {
				this->m_items = fn();

				if (this->m_items.empty())
					this->m_items = { "Empty" };

				const int min_selection = m_allow_no_selection ? -1 : 0;
				*this->m_var = std::clamp(*this->m_var, min_selection, static_cast<int>(this->m_items.size()) - 1);
			};

			return this;
		}

		c_listbox* color_stack(std::function<std::vector<hue::c_color>()> fn)
		{
			m_color_call_stack = std::move(fn);
			return this;
		}

		c_listbox* solid_dots_when_unselected()
		{
			m_solid_unselected_dots = true;
			return this;
		}

		c_listbox* allow_no_selection()
		{
			m_allow_no_selection = true;
			return this;
		}

		c_listbox* on_select(std::function<void(int)> fn)
		{
			m_on_select = std::move(fn);
			return this;
		}

		// 鼠标是否悬停在列表可滚动区域内(供父 child 判断:此区域滚动应只作用于列表)
		bool is_mouse_over_scroll_area() const
		{
			auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);
			return g_input->mouse_in_region(m_pos + position, math::c_vector_2d(m_child_size, m_height));
		}
	private:
		int* m_var{};
		std::vector<std::string> m_items{};
		std::vector<hue::c_color> m_item_colors{};
		std::function<std::vector<hue::c_color>()> m_color_call_stack{};
		std::function<void(int)> m_on_select{};
		float m_height{};
		bool m_solid_unselected_dots{ false };
		bool m_allow_no_selection{ false };

		bool m_listbox_item_hovered{false};
		float m_scroll_offset{ 0.f };
		float m_scroll_target{ 0.f };
	};
}
