#pragma once

namespace framework
{
	class c_colorpicker : public c_base_element
	{
	public:
		c_colorpicker(std::string label, hue::c_color* val, bool m_hide_label = true);

		void draw() override;
		void input() override;

		c_colorpicker* set_enabled(std::function<bool()> fn)
		{
			m_enabled = std::move(fn);
			return this;
		}
	private:
		hue::c_color* m_val{};
		hue::c_color m_default_val{};

		bool m_context_open{ false };
		bool m_context_just_opened{ false };
		bool m_palette_open{ false };
		math::c_vector_2d m_context_pos{};
		utils::anim_context_t m_context_menu_anim{};

		void rgb_to_hsv();
		void hsv_to_rgb();
		void apply_color(const hue::c_color& color);
		void draw_context_menu();

		float m_hue{}, m_saturation{}, m_value{};
		std::function<bool()> m_enabled{};

		bool enabled() const { return !m_enabled || m_enabled(); }
	};
}
