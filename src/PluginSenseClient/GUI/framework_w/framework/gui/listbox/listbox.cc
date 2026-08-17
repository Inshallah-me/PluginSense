#include "../../../includes.hh"

namespace framework
{
	c_listbox::c_listbox(std::string label, int* var, std::vector<std::string> items, float height, bool hide_label) : m_var(var), m_items(std::move(items)), m_height(height)
	{
		m_label = std::move(label);
		m_hide_label = hide_label;

		m_height = height;

		m_size = math::c_vector_2d(m_child_size, height + g_font->f_childs.measure(m_label).y + 5 + 8.f);
		m_type = framework::element_type::listbox;
		m_focus_priority = focus_priority::interactive;

		// we wont inline so there is no need for m_parent_width
	}

	void c_listbox::draw()
	{
		animations::m_listbox_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5f);
		// animations::m_listbox_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_listbox_value", m_visible &&  && g_ctx->m_open, 0.5f);
		animations::m_listbox_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_listbox_hover", m_visible && g_ctx->m_hovered == this, 0.5f);

		if (m_color_call_stack)
			m_item_colors = m_color_call_stack();

		const float height = m_height;

		// animation handling
		float target_opacity = 0.2f;
		//if (animations::m_listbox_value.val() > 0.f)
		//	target_opacity = 0.2f + (0.6f * animations::m_listbox_value.val());
		if (animations::m_listbox_hover.val() > 0.f)
			target_opacity = 0.2f + (0.6f * animations::m_listbox_hover.val());

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];

		float lerp_speed = 0.3f;
		smooth_opacity += (target_opacity - smooth_opacity) * lerp_speed;
		float final_opacity = animations::m_listbox_opacity.val() * smooth_opacity;

		if (!m_hide_label)
			g_font->f_childs.text(m_pos.x, m_pos.y - 0.5f, m_label, g_style->m_text.modulate(final_opacity));

		auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);

		g_render->rect_shadow((m_pos + position).x, (m_pos + position).y, m_child_size, height, g_style->m_window_shadow.modulate(animations::m_window_opacity.limit(0.3f).val()), 8.f, 3.f);
		animations::m_window_opacity.restore();
		g_render->rect_filled((m_pos + position).x, (m_pos + position).y, m_child_size, height, g_style->m_element_base.modulate(animations::m_window_opacity.val()), 3.f);

		// scrolling logic
		float visible_height = height - 10;
		float total_height = (g_font->f_childs.measure(m_label).y + 5) * this->m_items.size();
		float max_scroll = std::max(0.f, total_height - visible_height);

		bool listbox_hovered = g_input->mouse_in_region(m_pos + position, math::c_vector_2d(m_child_size, height));
		float scroll_delta = g_input->get_wheel_value();

		if (listbox_hovered && m_visible && g_ctx->m_open && max_scroll > 0.f && g_ctx->top_focus() == nullptr)
		{
			float scroll_speed = 40.f;
			float scroll_delta = g_input->get_wheel_value();
			m_scroll_target -= scroll_delta * scroll_speed;
			m_scroll_target = std::clamp(m_scroll_target, 0.f, max_scroll);
		}

		float lerp_speed_scroll = 0.15f;
		m_scroll_offset += (m_scroll_target - m_scroll_offset) * lerp_speed_scroll;
		if (std::abs(m_scroll_target - m_scroll_offset) < 0.5f)
		{
			m_scroll_offset = m_scroll_target;
		}

		// draw items
		g_render->push_clip((m_pos + position).x, (m_pos + position).y, m_child_size, height);

		float start_y = (m_pos + position).y + 5.f;
		for (int i = 0; i < this->m_items.size(); i++)
		{
			auto item = this->m_items[i];

			// item y
			float item_y = start_y + ((g_font->f_childs.measure(m_label).y + 5) * i) - m_scroll_offset;
			if (item_y + (g_font->f_childs.measure(m_label).y + 5) < (m_pos + position).y ||
				item_y > (m_pos + position).y + height)
				continue;

			animations::m_listbox_value = utils::builder::create_animation_ctx(m_parent + item + std::to_string(i) + "#m_listbox_value", m_visible && *this->m_var == i && g_ctx->m_open, 0.5f);

			// setup hovering data
			const float item_height = g_font->f_childs.measure(m_label).y + 5.f;
			m_listbox_item_hovered = g_input->mouse_in_region(
				math::c_vector_2d((m_pos + position).x + 6.f, item_y),
				math::c_vector_2d(m_child_size - 12.f, item_height));
			animations::m_listbox_item_hover = utils::builder::create_animation_ctx(m_parent + item + std::to_string(i) + "#m_listbox_item_hover", m_visible && this->m_listbox_item_hovered && g_ctx->m_open, 0.5f);

			// input
			if (this->m_listbox_item_hovered && g_input->clicked(input::mouse_buttons::left) && !g_ctx->m_click_consumed)
			{
				*this->m_var = i;
				if (m_on_select)
					m_on_select(i);
				g_ctx->m_click_consumed = true;
			}

			if (m_solid_unselected_dots)
			{
				auto unselected_color = i < m_item_colors.size() ? m_item_colors[i] : g_style->m_text;
				auto dot_color = unselected_color.modulate(animations::m_window_opacity.limit(0.25f).val())
					.lerp(g_style->m_accent.modulate(animations::m_window_opacity.limit(1.f).val()), animations::m_listbox_value.val());
				g_render->rect_filled(m_pos.x + 6, item_y + 3, 11, 11, dot_color, 50.f);
			}
			else
			{
				g_render->rect(m_pos.x + 6, item_y + 3, 11, 11, hue::c_color().modulate(0.3f), 50.f);
				g_render->rect_filled(m_pos.x + 6, item_y + 3, 11, 11, g_style->m_accent.modulate(animations::m_listbox_value.limit(1.f).val()), 50.f);
			}
			//g_font->f_icons_rs.text(m_pos.x + 6, item_y, icon-fa_cirlc, hue::c_color());

			g_font->f_childs.text((m_pos + position).x + 22 + (6 * animations::m_listbox_item_hover.val()), item_y, item,
				g_style->m_text.modulate(animations::m_window_opacity.limit(0.3f).val()).lerp(g_style->m_accent.modulate(animations::m_window_opacity.limit(1.f).val()), animations::m_listbox_value.val()));
		}

		g_render->restore_clip();

		// scrollbar(同分区/child 样式)
		if (max_scroll > 0.f)
		{
			const float track_x = (m_pos + position).x + m_child_size - 7.f;
			const float track_y = (m_pos + position).y + 5.f;
			const float track_h = height - 10.f;
			const float thumb_h = std::clamp((track_h / std::max(total_height, track_h)) * track_h, 18.f, track_h);
			const float thumb_range = std::max(1.f, track_h - thumb_h);
			const float thumb_y = track_y + (m_scroll_offset / std::max(1.f, max_scroll)) * thumb_range;
			const float alpha = animations::m_listbox_opacity.val();

			g_render->rect_filled(track_x, track_y, 2.f, track_h, hue::c_color(70, 70, 70).modulate(alpha * 0.35f), 2.f);
			g_render->rect_filled(track_x, thumb_y, 2.f, thumb_h, hue::c_color(125, 125, 125).modulate(alpha * 0.55f), 2.f);
		}
	}

	void c_listbox::input()
	{
		math::c_rect bounding = math::c_rect(m_pos, m_size);

		if (!g_ctx->can_interact(this, m_focus_priority))
			return;

		if (g_input->mouse_in_region(bounding.pos(), bounding.size()))
		{
			g_ctx->m_hovered = this;
		}
		else if (g_ctx->m_hovered == this)
		{
			g_ctx->m_hovered = nullptr;
		}

		if (g_ctx->m_hovered == this && g_input->clicked(input::mouse_buttons::left))
		{
			// *this->m_value = !*this->m_value;
		}
	}
}
