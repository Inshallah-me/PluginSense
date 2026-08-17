#include "../../../includes.hh"

namespace framework
{
	c_checkbox::c_checkbox(std::string label, bool* value): m_value(value) 
	{
		m_label = std::move(label);
		m_size = math::c_vector_2d(0, 19);

		m_type = element_type::checkbox;
		m_focus_priority = focus_priority::interactive;

		// this element width (parent width named as it will be only accessed from parent)
		m_parent_width = (30 + g_font->f_childs.measure(m_label).x);
	}

	void c_checkbox::draw()
	{
		auto size = math::c_vector_2d(0, 17);
		const bool can_use = enabled();
		const float enabled_alpha = can_use ? 1.f : 0.45f;

		animations::m_checkbox_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5f);
		animations::m_checkbox_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_checkbox_value", m_visible && *this->m_value && g_ctx->m_open, 0.5f);
		animations::m_checkbox_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_checkbox_hover", m_visible && can_use && g_ctx->m_hovered == this, 0.5f);

		// animation handling
		float target_opacity = 0.2f;
		if (animations::m_checkbox_value.val() > 0.f)
			target_opacity = 0.2f + (0.6f * animations::m_checkbox_value.val());
		else if (animations::m_checkbox_hover.val() > 0.f)
			target_opacity = 0.2f + (0.2f * animations::m_checkbox_hover.val());

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];

		float lerp_speed = 0.3f;
		smooth_opacity += (target_opacity - smooth_opacity) * lerp_speed;
		float final_opacity = animations::m_checkbox_opacity.val() * smooth_opacity * enabled_alpha;

		g_render->use_layer(m_layer, [&]()
			{
				g_render->rect_shadow(m_pos.x, m_pos.y, size.y, size.y, g_style->m_window_shadow.modulate(animations::m_window_opacity.limit(0.3f).val() * enabled_alpha).lerp(g_style->m_accent.modulate(animations::m_window_opacity.limit(0.4f).val() * enabled_alpha), animations::m_checkbox_value.val()), 8.f, 3.f);
				animations::m_window_opacity.restore();
				g_render->rect_filled(m_pos.x, m_pos.y, size.y, size.y, g_style->m_element_base.modulate(animations::m_window_opacity.val() * enabled_alpha).lerp(g_style->m_accent.modulate(enabled_alpha), animations::m_checkbox_value.val()), 3.f);
				g_render->fade_rect_filled(m_pos.x + 1, m_pos.y + 1, size.y - 2, size.y - 2, hue::c_color(0, 0, 0, 0), hue::c_color(0, 0, 0, 50 * animations::m_checkbox_value.val() * enabled_alpha), engine::fade_direction::vertically, 3.f);
				g_render->check_mark(m_pos.x + 5, m_pos.y + 5, 8.f, hue::c_color(0, 0, 0, 255 * animations::m_checkbox_value.val() * enabled_alpha));

				g_font->f_childs.text(m_pos.x + 25, m_pos.y, m_label, g_style->m_text.modulate(final_opacity));
			});
	}

	void c_checkbox::input()
	{
		// input system
		// size test as m_size.x is not set
		// note to handle it
		math::c_rect bounding = math::c_rect(m_pos, math::c_vector_2d(m_size.y + 7 + g_font->f_childs.measure(m_label).x, m_size.y));

		if (!enabled())
		{
			if (g_ctx->m_hovered == this)
				g_ctx->m_hovered = nullptr;
			return;
		}

		if (!g_ctx->can_interact(this, m_focus_priority))
			return;

		if (g_input->mouse_in_region(bounding.pos(), bounding.size()))
		{
			g_ctx->m_hovered = this;
		}
		else if (g_ctx->m_hovered == this) // we need to make sure the hold is inherited from this
		{
			g_ctx->m_hovered = nullptr;
		}
		
		if (g_ctx->m_hovered == this && g_input->clicked(input::mouse_buttons::left) && !g_ctx->m_click_consumed)
		{
			*this->m_value = !*this->m_value;
			if (m_on_change)
				m_on_change(*this->m_value);
			g_ctx->m_click_consumed = true;
		}
	}
}
