#include "../../../includes.hh"

#include <PluginSenseClient/Settings/MenuState.hpp>

void framework::c_widgets::draw()
{
	if (this->m_widgets.empty())
		return;

	this->m_panel_anim = utils::builder::create_animation_ctx("panel_anim", this->m_has_focus != nullptr, 0.5f);

	g_render->rect_filled(0.f, 0.f, static_cast<float>(core::g_overlay->width), static_cast<float>(core::g_overlay->height), hue::c_color(0, 0, 0).modulate(this->m_panel_anim.limit(0.15f).val()));

	auto clear_focus = [this](const std::shared_ptr<c_widget_base>& widget)
	{
		if (widget && widget->focused == widget.get())
		{
			widget->focused = nullptr;
			this->m_has_focus = nullptr;
			widget->m_widget_dragged = false;
		}
	};

	auto draw_widget = [this, &clear_focus](const std::shared_ptr<c_widget_base>& widget)
	{
		if (!widget)
			return;

		auto clamp_widget_to_screen = [](const std::shared_ptr<c_widget_base>& item)
		{
			constexpr float focus_padding = 4.f;
			const float max_x = std::max(focus_padding, static_cast<float>(core::g_overlay->width) - item->m_size.x - focus_padding);
			const float max_y = std::max(focus_padding, static_cast<float>(core::g_overlay->height) - item->m_size.y - focus_padding);
			item->m_pos.x = std::clamp(item->m_pos.x, focus_padding, max_x);
			item->m_pos.y = std::clamp(item->m_pos.y, focus_padding, max_y);
		};

		clamp_widget_to_screen(widget);

		if (g_ctx->m_open)
		{
			// widget focusing logic
			math::c_rect bounding = math::c_rect(widget->m_pos, widget->m_size);
			if (g_input->mouse_in_region(bounding.pos(), bounding.size()) && g_input->click_down(input::mouse_buttons::left))
			{
				widget->focused = widget.get();
				this->m_has_focus = widget.get();
			}
			else if (widget->focused == widget.get() && !g_input->click_down(input::mouse_buttons::left))
			{
				widget->focused = nullptr;
				this->m_has_focus = nullptr;
				widget->m_widget_dragged = false;
			}

			// widget moving logic
			if (widget->focused)
			{
				// update everyframe 
				widget->delta = widget->prev_mouse_pos - g_input->get_mouse_position();

				// we do not to check for bounding as it is valid
				if (!widget->m_widget_dragged)
				{
					widget->m_widget_dragged = true;
				}
				else if (widget->m_widget_dragged)
				{
					widget->m_pos -= widget->delta;
					clamp_widget_to_screen(widget);
				}

				widget->prev_mouse_pos = g_input->get_mouse_position();
			}
		}
		else
		{
			clear_focus(widget);
		}

		widget->input();
		clamp_widget_to_screen(widget);
		widget->draw();
	};

	if (this->m_manager && vars::menuKeybinds)
		draw_widget(this->m_manager);
	else
		clear_focus(this->m_manager);

	if (this->m_notify_panel)
		draw_widget(this->m_notify_panel);
}

void framework::c_widgets::create_widget(widget_type type, math::c_vector_2d& pos)
{
	switch (type)
	{
		case widget_type::keybind:
		{
			auto control = std::make_shared<c_keybinds>(std::move(pos), std::vector<keybind_entry_t>{});
			this->m_manager = control;
			m_widgets.push_back(control);

		}
		break;
		case widget_type::notification_panel:
		{
			auto control = std::make_shared<c_notify_panel>(std::move(pos));
			this->m_notify_panel = control;
			m_widgets.push_back(control);
		}
		break;

	}
}

framework::c_keybinds::c_keybinds(math::c_vector_2d base_pos, std::vector<keybind_entry_t> keybinds)
{
	m_pos = std::move(base_pos);
	this->keybinds = std::move(keybinds);

	m_size = math::c_vector_2d(120, 30);
}

void framework::c_keybinds::draw()
{
	if (keybinds.empty())
	{
		m_size = math::c_vector_2d(0, 0);
		return;
	}

	float base_width = 120;
	auto static_width = std::clamp(keybinds.empty() ? base_width : get_biggest_size_keybinds(keybinds) + 20, base_width, 300.f);

	static auto interpolated_width = base_width;
	if (interpolated_width != static_width) {
		interpolated_width += (static_width - interpolated_width) * 0.1f;
	}

	float base_height = 40;
	auto static_height = std::clamp(keybinds.empty() ? base_height : base_height + (22 * keybinds.size()), base_height, 500.f);

	static auto interpolated_height = base_height;
	if (interpolated_height != static_height) {
		interpolated_height += (static_height - interpolated_height) * 0.1f;
	}
	math::c_vector_2d rect_size = math::c_vector_2d(interpolated_width, interpolated_height);
	m_size = rect_size;

	g_render->rect_shadow(m_pos.x, m_pos.y, rect_size.x, rect_size.y, framework::g_style->m_window_shadow.modulate(0.3f), 15.f, 5.f);
	g_render->rect_filled(m_pos.x, m_pos.y, rect_size.x, rect_size.y, framework::g_style->m_window_background, 5.f);

	math::c_vector_2d icon_font = g_font->f_icons.measure(ICON_FA_KEYBOARD) + math::c_vector_2d(0, 0);
	g_font->f_icons.text(m_pos.x + 7, m_pos.y + (30 * 0.5f) - (icon_font.y * 0.5f) - 1, ICON_FA_KEYBOARD, framework::g_style->m_accent);
	g_font->f_childs.text(m_pos.x + 9 + icon_font.x + 5, m_pos.y + (30 * 0.5f) - (g_font->f_childs.measure("plm").y * 0.5f), "Keybinds", framework::g_style->m_text.modulate(0.6f));

	g_render->push_clip(m_pos.x, m_pos.y, rect_size.x, 30);
	g_render->rect_shadow(m_pos.x + 45, m_pos.y + 14, 20, 2, g_style->m_accent.modulate(0.15f), 90.f, 0.f);
	g_render->restore_clip();

	g_render->gradient(this->m_pos.x, this->m_pos.y + 30, this->m_size.x, 10, g_style->m_window_shadow.modulate(0.2f), g_style->m_window_shadow.modulate(0.1f).with_alpha(0), engine::fade_direction::horizontally);

	float y = m_pos.y + 35.f;
	for (auto& entry : keybinds)
	{
		auto entry_anim = utils::builder::create_animation_ctx(
			"keybind_entry::" + entry.m_label, true, 0.5f);

		float t = entry_anim.val();
		float eased = t * t * (3.f - 2.f * t);

		auto pos = math::c_vector_2d(m_pos.x + 7, y);
		std::string icon;
		float widget_x = 0.f;
		float widget_y = 0.f;

		if (!entry.m_icon_override.empty())
		{
			widget_x = g_font->f_icons.measure(entry.m_icon_override.c_str()).x;
			icon = entry.m_icon_override;
			widget_y = 2.f;
		}
		else switch (entry.m_type)
		{
		case widget_mode::always:
			widget_x = g_font->f_icons.measure(ICON_FA_CIRCLE_EXCLAMATION).x;
			icon = ICON_FA_CIRCLE_EXCLAMATION;
			widget_y = 2.f;
			break;
		case widget_mode::hold:
			widget_x = g_font->f_icons.measure(ICON_FA_HAND_HOLDING).x;
			icon = ICON_FA_HAND_HOLDING;
			widget_y = -1.f;
			break;
		case widget_mode::toggle:
			widget_x = g_font->f_icons.measure(ICON_FA_TOGGLE_ON).x;
			icon = ICON_FA_TOGGLE_ON;
			widget_y = 2.f;
			break;
		}

		g_font->f_icons.text(pos.x, pos.y + widget_y, icon,
			framework::g_style->m_accent.modulate(eased));
		g_font->f_childs.text(pos.x + widget_x + 5, pos.y + 1, entry.m_label,
			framework::g_style->m_text.modulate(0.6f * eased));

		y += 22.f;
	}

	// focus rect
	g_render->dashed_rect(m_pos.x - 2, m_pos.y - 2, rect_size.x + 4, rect_size.y + 4, framework::g_style->m_accent.modulate(m_focus_anim.val()),
		5.f, 1.f, 5.f, 5.f);
}

void framework::c_keybinds::input()
{
	m_focus_anim = utils::builder::create_animation_ctx("keybind_focus", this->focused == this, 0.5f);
}

framework::c_notify_panel::c_notify_panel(math::c_vector_2d base_pos)
{
	m_pos = std::move(base_pos);
	m_size = math::c_vector_2d(300, 400);
}

void framework::c_notify_panel::draw()
{

	float dt = ImGui::GetIO().DeltaTime;
	float padding_x = 5.f;
	float padding_y = 8.f;
	float slide_speed_in = 8.f;
	float slide_speed_out = 6.f;

	//float max_w = 0.f;
	for (auto& entry : m_entries) {
		float w = g_font->f_childs.measure(entry.text).x + g_font->f_icons.measure(ICON_FA_BELL).x;
		for (auto& seg : entry.inlines)
			w += g_font->f_childs.measure(seg.text).x + 1;


		entry.m_max_width = std::max(entry.m_max_width, w + 18);
	}

	for (int i = 0; i < (int)m_entries.size(); i++)
	{
		auto& e = m_entries[i];
		e.m_lifetime += dt;

		if (!e.m_initialized) {
			e.m_slide_anim = 0.f;
			e.m_initialized = true;
		}

		bool sliding_out = e.m_lifetime >= e.m_max_life;

		if (!sliding_out)
			e.m_slide_anim += (1.f - e.m_slide_anim) * dt * slide_speed_in;
		else
			e.m_slide_anim += (0.f - e.m_slide_anim) * dt * slide_speed_out;

		if (sliding_out && e.m_slide_anim < 0.01f)
		{
			m_entries.erase(m_entries.begin() + i);
			i--;
		}
	}

	float cursor_y = m_pos.y + padding_y;
	for (auto& entry : m_entries)
	{
		float t = entry.m_slide_anim;
		float eased = t * t * (3.f - 2.f * t);

		float rest_x = m_pos.x + padding_x;
		float slide_offset = (1.f - eased) * (entry.m_max_width + 40.f);
		float draw_x = rest_x - slide_offset;

		g_render->rect_shadow(draw_x, cursor_y, entry.m_max_width, 30.f, framework::g_style->m_window_shadow.modulate(0.3f * eased), 15.f, 5.f);
		g_render->rect_filled(draw_x, cursor_y, entry.m_max_width, 30.f, framework::g_style->m_window_background.modulate(eased), 5.f);

		g_render->push_clip(draw_x, cursor_y, entry.m_max_width, 30);
		g_render->rect_shadow(draw_x + 40, cursor_y + 14, 20, 2, g_style->m_accent.modulate(0.15f * eased), 90.f, 0.f);
		g_render->restore_clip();

		g_font->f_icons.text(draw_x + 7, cursor_y + (30 * 0.5f) - (g_font->f_icons.measure(ICON_FA_BELL).y * 0.5f) - 1, ICON_FA_BELL, framework::g_style->m_accent.modulate(eased));

		auto builder = g_font->f_childs.text(draw_x + g_font->f_icons.measure(ICON_FA_BELL).x + 15, cursor_y + (30 * 0.5f) - (g_font->f_childs.measure("smth").y * 0.5f), entry.text, entry.color.modulate(eased), entry.flags, entry.shadow_color);
		for (auto& seg : entry.inlines)
			builder.inlined(seg.text, seg.color.modulate(eased));

		cursor_y += 40.f * eased;
	}

	g_font->f_icons.text(m_pos.x + padding_x, m_pos.y - 20, ICON_FA_CIRCLE_EXCLAMATION, framework::g_style->m_text.modulate(this->m_panel_dash.limit(0.6f).val()));
	g_font->f_childs.text(m_pos.x + padding_x + g_font->f_icons.measure(ICON_FA_CIRCLE_EXCLAMATION).x + 5, m_pos.y - 20, "Notifications", framework::g_style->m_text.modulate(this->m_panel_dash.limit(0.6f).val()));

	// focus rect
	g_render->dashed_rect(m_pos.x - 2, m_pos.y - 2, m_size.x, m_size.y, framework::g_style->m_text.modulate(this->m_panel_dash.limit(0.6f).val()).lerp(g_style->m_accent.modulate(this->m_panel_dash.limit(0.9f).val()), m_focus_anim.val()),
		5.f, 1.f, 5.f, 5.f);
}

void framework::c_notify_panel::input()
{
	m_focus_anim = utils::builder::create_animation_ctx("notify_focus", this->focused == this, 0.5f);
	m_panel_dash = utils::builder::create_animation_ctx("m_panel_dash", g_ctx->m_open, 0.5f);
}


/*

	float line_spacing = 4.f;
	float line_height = g_font->f_childs.measure("A").y;

	// compute total height
	float total_h = padding_y * 2.f + m_entries.size() * line_height
		+ (m_entries.size() > 1 ? (m_entries.size() - 1) * line_spacing : 0.f);

	// draw dashed border
	g_render->dashed_rect(m_pos.x, m_pos.y, max_w, total_h,
		hue::c_color(200, 220, 255, 200), 12.f, 1.5f, 8.f, 5.f);

*/
