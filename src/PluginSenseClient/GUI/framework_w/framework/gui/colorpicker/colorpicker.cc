#include "../../../includes.hh"

namespace framework
{
	namespace
	{
		hue::c_color g_colorpicker_clipboard{};
		bool g_colorpicker_clipboard_valid{ false };

		float smoothstep(float t)
		{
			t = std::clamp(t, 0.f, 1.f);
			return t * t * (3.f - 2.f * t);
		}

		void close_context_menu(c_colorpicker* picker, bool consume_click)
		{
			g_ctx->pop_focus(picker);
			if (g_ctx->m_hovered == picker)
				g_ctx->m_hovered = nullptr;
			if (consume_click)
				g_ctx->m_click_consumed = true;
		}
	}

	c_colorpicker::c_colorpicker(std::string label, hue::c_color* val, bool hide_label) : m_val(val)
	{
		m_label = std::move(label);
		m_default_val = m_val ? *m_val : hue::c_color();
		m_hide_label = hide_label;
		m_size = { 0, (m_hide_label ? 0.f : 16.f) };

		m_type = element_type::colorpicker;
		m_focus_priority = focus_priority::persistent;

		// we only use this in terms of inlining elements
		m_parent_width = 20;

		// wtv
		this->rgb_to_hsv();
	}

	void c_colorpicker::draw()
	{
		const bool can_use = enabled();
		const float enabled_alpha = can_use ? 1.f : 0.45f;

		if (!m_val)
			return;

		animations::m_colorpicker_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5f);
		animations::m_colorpicker_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_colorpicker_value", m_visible && m_palette_open && g_ctx->is_focused(this) && g_ctx->m_open, 0.5f);
		animations::m_colorpicker_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_colorpicker_hover", m_visible && can_use && g_ctx->m_hovered == this, 0.5f);

		// animation handling
		float target_opacity = 0.2f;
		if (animations::m_colorpicker_value.val() > 0.f)
			target_opacity = 0.2f + (0.6f * animations::m_colorpicker_value.val());
		else if (animations::m_colorpicker_hover.val() > 0.f)
			target_opacity = 0.2f + (0.2f * animations::m_colorpicker_hover.val());

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];

		float lerp_speed = 0.3f;
		smooth_opacity += (target_opacity - smooth_opacity) * lerp_speed;
		float final_opacity = animations::m_colorpicker_opacity.val() * smooth_opacity * enabled_alpha;

		g_render->use_layer(m_layer, [&]()
			{
				if (!m_hide_label)
				{
					// data when hide label is false its diff, if we're gonna inline elements atleast do it properly
					g_font->f_childs.text(m_pos.x, m_pos.y - 0.5f, m_label, g_style->m_text.modulate(final_opacity));
					g_font->f_icons.text(m_pos.x + (m_child_size - g_font->f_icons.measure(ICON_FA_PALLET).x) + 2, m_pos.y, ICON_FA_PALETTE, this->m_val->modulate(final_opacity));
				}
				else
				{
					// imagine if we have multiinline
					g_font->f_icons.text(m_pos.x, m_pos.y, ICON_FA_PALETTE, this->m_val->modulate(final_opacity));
				}
			});
	
		if (animations::m_colorpicker_value.val() > 0.f)
		{
			math::c_vector_2d pallete_size = { 180, 220 };

			// position based on the label status
			math::c_vector_2d pallete_pos = !m_hide_label
				? math::c_vector_2d(m_pos.x + m_child_size - (g_font->f_icons.measure(ICON_FA_PALETTE).x), m_pos.y + 20)
				: math::c_vector_2d(m_pos.x, m_pos.y + 20);

			float t = animations::m_colorpicker_value.val();
			float eased = t * t * (3.f - 2.f * t);

			float origin_y = pallete_pos.y;
			float origin_x = pallete_pos.x;

			ImDrawList* dl = ImGui::GetForegroundDrawList();
			int vtx_start = dl->VtxBuffer.Size;

			g_render->use_layer(engine::render_layer::prioritized, [&]()
			{
					// draw the background of the pallete
					g_render->rect_shadow(pallete_pos.x, pallete_pos.y, pallete_size.x, pallete_size.y, g_style->m_window_shadow.modulate(animations::m_colorpicker_value.val()), 10.f, 5.f);
					g_render->rect_filled(pallete_pos.x, pallete_pos.y, pallete_size.x, pallete_size.y, g_style->m_window_background.modulate(animations::m_colorpicker_value.val()), 5.f);

					g_render->gradient(pallete_pos.x, pallete_pos.y + 25.f, pallete_size.x, 10, g_style->m_window_shadow.modulate(animations::m_colorpicker_value.limit(0.2f).val()), g_style->m_window_shadow.modulate(animations::m_colorpicker_value.limit(0.1f).val()).with_alpha(0), engine::fade_direction::horizontally);

				// pallete name
				g_font->f_childs.text(pallete_pos.x + 8, pallete_pos.y + 3, m_label, g_style->m_text.modulate(animations::m_colorpicker_value.limit(0.6f).val()));
				animations::m_colorpicker_value.restore();

				// get pure hsv color
				float h_normalized = m_hue / 360.f;
				int hue_r, hue_g, hue_b;

				if (h_normalized < 1.f / 6.f) {
					hue_r = 255; hue_g = (int)(255 * h_normalized * 6.f); hue_b = 0;
				}
				else if (h_normalized < 2.f / 6.f) {
					hue_r = (int)(255 * (2.f / 6.f - h_normalized) * 6.f); hue_g = 255; hue_b = 0;
				}
				else if (h_normalized < 3.f / 6.f) {
					hue_r = 0; hue_g = 255; hue_b = (int)(255 * (h_normalized - 2.f / 6.f) * 6.f);
				}
				else if (h_normalized < 4.f / 6.f) {
					hue_r = 0; hue_g = (int)(255 * (4.f / 6.f - h_normalized) * 6.f); hue_b = 255;
				}
				else if (h_normalized < 5.f / 6.f) {
					hue_r = (int)(255 * (h_normalized - 4.f / 6.f) * 6.f); hue_g = 0; hue_b = 255;
				}
				else {
					hue_r = 255; hue_g = 0; hue_b = (int)(255 * (1.f - h_normalized) * 6.f);
				}

				// hsv pos
				math::c_vector_2d hsv_pos = { pallete_pos.x + 8, pallete_pos.y + 33 };
				math::c_vector_2d hsv_size = { pallete_size.x - 16, 180 - 41 };

				g_render->rect_filled(hsv_pos.x, hsv_pos.y, hsv_size.x, hsv_size.y, hue::c_color(hue_r, hue_g, hue_b).modulate(animations::m_colorpicker_value.val()), 2.f);
				g_render->gradient(hsv_pos, hsv_size, hue::c_color(255, 255, 255, 255 * animations::m_colorpicker_value.val()), hue::c_color(255, 255, 255, 0), engine::fade_direction::vertically, 2.f, g_style->m_window_bars.modulate(animations::m_colorpicker_value.val()));
				g_render->gradient(hsv_pos, hsv_size, hue::c_color(0, 0, 0, 0), hue::c_color(0, 0, 0, 255 * animations::m_colorpicker_value.val()), engine::fade_direction::horizontally, 2.f, g_style->m_window_bars.modulate(animations::m_colorpicker_value.val()));

				// hue bar
				math::c_vector_2d hue_pos = { hsv_pos.x, hsv_pos.y + hsv_size.y + 10 };
				math::c_vector_2d hue_size = { hsv_size.x, 10 };

				int segment_count = 6;
				for (int i = 0; i < segment_count; i++) {
					float segment_width = hsv_size.x / (float)segment_count;
					float segment_x = hsv_pos.x + static_cast<float>(i) * segment_width;
					float next_x = hsv_pos.x + static_cast<float>(i + 1) * segment_width;

					hue::c_color color1, color2;
					switch (i) {
					case 0: color1 = hue::c_color(255, 0, 0); color2 = hue::c_color(255, 255, 0); break;
					case 1: color1 = hue::c_color(255, 255, 0); color2 = hue::c_color(0, 255, 0); break;
					case 2: color1 = hue::c_color(0, 255, 0); color2 = hue::c_color(0, 255, 255); break;
					case 3: color1 = hue::c_color(0, 255, 255); color2 = hue::c_color(0, 0, 255); break;
					case 4: color1 = hue::c_color(0, 0, 255); color2 = hue::c_color(255, 0, 255); break;
					case 5: color1 = hue::c_color(255, 0, 255); color2 = hue::c_color(255, 0, 0); break;
					}

					g_render->gradient(math::c_vector_2d(segment_x, hue_pos.y), math::c_vector_2d(next_x - segment_x, 10.f), color1.modulate(animations::m_colorpicker_value.val()), color2.modulate(animations::m_colorpicker_value.val()),
						engine::fade_direction::vertically, (i == 0 || i == 5) ? 2.f : 0.f, g_style->m_window_bars.modulate(animations::m_colorpicker_value.val()), ((i == 0 ? engine::draw_flags_::draw_flags_round_corners_left : 0) |
							(i == 5 ? engine::draw_flags_::draw_flags_round_corners_right : 0)));
				}

				// alpha bar
				math::c_vector_2d alpha_bar = { hue_pos.x, hue_pos.y + 20 };
				math::c_vector_2d alpha_size = { hsv_size.x, 10 };

				const float r = 3.f;
				const int total_cols = static_cast<int>((alpha_size.x + 1.f) / 5.f);

				for (int i = 0; i < total_cols; i++) {
					for (int j = 0; j < 2; j++) {
						bool is_light = (i + j) % 2 == 0;
						auto color = hue::c_color(is_light ? 200 : 150, is_light ? 200 : 150, is_light ? 200 : 150)
							.modulate(animations::m_colorpicker_value.val());

						engine::draw_flags flags = engine::draw_flags_round_corners_none;
						float tile_r = 0.f;

						if (i == 0) {
							flags = engine::draw_flags_round_corners_left;
							tile_r = r;
						}
						else if (i == total_cols - 1) {
							flags = engine::draw_flags_round_corners_right;
							tile_r = r;
						}

						g_render->rect_filled(
							alpha_bar.x + (i * 5), alpha_bar.y + (j * 5), 5, 5,
							color, tile_r, flags
						);
					}
				}

				g_render->gradient(math::c_vector_2d(alpha_bar.x, alpha_bar.y), alpha_size, hue::c_color(this->m_val->r, this->m_val->g, this->m_val->b, 0), hue::c_color(this->m_val->r, this->m_val->g, this->m_val->b, 255).modulate(animations::m_colorpicker_value.val()),engine::fade_direction::vertically, 0);

				// cursor data - remake this shit
				{
					float cursor_x = hsv_pos.x + m_saturation * hsv_size.x;
					float cursor_y = hsv_pos.y + (1.f - m_value) * hsv_size.y;
					float cursor_size = 8.f;

					g_render->rect(cursor_x - cursor_size / 2, cursor_y - cursor_size / 2, cursor_size, cursor_size, hue::c_color(255, 255, 255).modulate(animations::m_colorpicker_value.val()), 4);
					g_render->rect(cursor_x - cursor_size / 2 + 1, cursor_y - cursor_size / 2 + 1, cursor_size - 2, cursor_size - 2, hue::c_color(0, 0, 0).modulate(animations::m_colorpicker_value.val()), 4);
				}

				{
					float hue_cursor_x = hue_pos.x + (m_hue / 360.f) * hue_size.x;
					float hue_cursor_width = 4.f;
					g_render->rect_filled(hue_cursor_x - hue_cursor_width / 2, hue_pos.y - (2 ), hue_cursor_width, hue_size.y + (4),
						hue::c_color(255, 255, 255).modulate(animations::m_colorpicker_value.val()), 2.f);
					g_render->rect(hue_cursor_x - hue_cursor_width / 2, hue_pos.y - (2 ), hue_cursor_width, hue_size.y + (4),
						hue::c_color(0, 0, 0).modulate(animations::m_colorpicker_value.val()), 2.f);
				}

				{
					float alpha_cursor_x = alpha_bar.x + (this->m_val->a / 255.f) * alpha_size.x;
					float alpha_cursor_width = 4.f;
					g_render->rect_filled(alpha_cursor_x - alpha_cursor_width / 2, alpha_bar.y - (2), alpha_cursor_width, alpha_size.y + (4),
						hue::c_color(255, 255, 255).modulate(animations::m_colorpicker_value.val()), 2.f);
					g_render->rect(alpha_cursor_x - alpha_cursor_width / 2, alpha_bar.y - (2), alpha_cursor_width, alpha_size.y + (4),
						hue::c_color(0, 0, 0).modulate(animations::m_colorpicker_value.val()), 2.f);
				}

				//g_render->rect(pallete_pos.x - 1, pallete_pos.y - 1, pallete_size.x + 2, pallete_size.y + 2, g_style->m_outline.modulate(animations::m_colorpicker_value.val()), 4.f);
			}, true);

			int vtx_end = dl->VtxBuffer.Size;

			ImDrawVert* verts = dl->VtxBuffer.Data;
			for (int v = vtx_start; v < vtx_end; v++)
			{
				verts[v].pos.x = origin_x + (verts[v].pos.x - origin_x) * eased;
				verts[v].pos.y = origin_y + (verts[v].pos.y - origin_y) * eased;

				int a = (int)(((verts[v].col >> IM_COL32_A_SHIFT) & 0xFF) * eased);
				verts[v].col = (verts[v].col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
			}

			// check if we click oob
			math::c_rect bounding_data = math::c_rect(pallete_pos, pallete_size);

			if (!g_input->mouse_in_region(bounding_data.pos(), bounding_data.size()) && g_input->clicked(input::mouse_buttons::left))
			{
				m_palette_open = false;
				g_ctx->pop_focus(this);
				g_ctx->m_click_consumed = true;
			}
		}

		this->draw_context_menu();
	}

	void c_colorpicker::input()
	{
		if (!m_val)
			return;

		const auto icon_size = g_font->f_icons.measure(ICON_FA_PALLET);
		math::c_rect bounding = math::c_rect(m_pos, math::c_vector_2d(m_hide_label ? icon_size.x : m_child_size, m_hide_label ? icon_size.y : m_size.y));

		if (!enabled())
		{
			if (g_ctx->m_hovered == this)
				g_ctx->m_hovered = nullptr;
			if (g_ctx->is_focused(this))
				g_ctx->pop_focus(this);
			m_context_open = false;
			m_palette_open = false;
			return;
		}

		if (!g_ctx->can_interact(this, m_focus_priority))
			return;

		if (g_input->mouse_in_region(bounding.pos(), bounding.size()))
		{
			g_ctx->m_hovered = this;
		}
		else if (g_ctx->m_hovered == this) {
			g_ctx->m_hovered = nullptr;
		}

		if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this && !g_ctx->m_click_consumed)
		{
			m_palette_open = true;
			m_context_open = false;
			g_ctx->push_focus(this, m_focus_priority); // we opened it mfucker
		}

		if (m_context_open)
		{
			const math::c_vector_2d menu_size = { 70.f, 75.f };
			math::c_rect menu_rect(m_context_pos, menu_size);
			if (g_input->mouse_in_region(menu_rect.pos(), menu_rect.size()))
			{
				g_ctx->m_hovered = this;
				if (g_input->clicked(input::mouse_buttons::left) || g_input->clicked(input::mouse_buttons::right))
					g_ctx->m_click_consumed = true;
			}
		}

		if (g_input->clicked(input::mouse_buttons::right) && g_ctx->m_hovered == this && !g_ctx->m_click_consumed)
		{
			m_context_open = true;
			m_context_just_opened = true;
			m_palette_open = false;
			m_context_pos = g_input->get_mouse_position() + math::c_vector_2d(3.f, 3.f);
			g_ctx->push_focus(this, m_focus_priority);
			g_ctx->m_click_consumed = true;
		}

		if (m_palette_open && g_ctx->is_focused(this))
		{
			// inside colorpicker 
			math::c_vector_2d pallete_size = { 180, 230 };

			// position based on the label status
			math::c_vector_2d pallete_pos = !m_hide_label
				? math::c_vector_2d(m_pos.x + m_child_size - (g_font->f_icons.measure(ICON_FA_PALETTE).x), m_pos.y + 20)
				: math::c_vector_2d(m_pos.x, m_pos.y + 20);

			// hsv pos
			math::c_vector_2d hsv_pos = { pallete_pos.x + 8, pallete_pos.y + 33 };
			math::c_vector_2d hsv_size = { pallete_size.x - 16, 180 - 41 };

			// hue bar
			math::c_vector_2d hue_pos = { hsv_pos.x, hsv_pos.y + hsv_size.y + 10 };
			math::c_vector_2d hue_size = { hsv_size.x, 10 };

			// alpha bar
			math::c_vector_2d alpha_bar = { hue_pos.x, hue_pos.y + 20 };
			math::c_vector_2d alpha_size = { hsv_size.x, 10 };

			if (g_input->click_down(input::mouse_buttons::left) &&
				g_input->mouse_in_region(hsv_pos, hsv_size)) {

				auto mouse_pos = g_input->get_mouse_position();
				m_saturation = std::clamp((mouse_pos.x - hsv_pos.x) / (float)hsv_size.x, 0.f, 1.f);
				m_value = std::clamp(1.f - (mouse_pos.y - hsv_pos.y) / (float)hsv_size.y, 0.f, 1.f);
				hsv_to_rgb();
			}

			if (g_input->click_down(input::mouse_buttons::left) &&
				g_input->mouse_in_region(hue_pos, hue_size)) {

				auto mouse_pos = g_input->get_mouse_position();
				m_hue = std::clamp((mouse_pos.x - hue_pos.x) / (float)hue_size.x, 0.f, 1.f) * 360.f;
				hsv_to_rgb();
			}

			if (g_input->click_down(input::mouse_buttons::left) &&
				g_input->mouse_in_region(alpha_bar, alpha_size)) {

				auto mouse_pos = g_input->get_mouse_position();
				this->m_val->a = (int)(std::clamp((mouse_pos.x - alpha_bar.x) / (float)alpha_size.x, 0.f, 1.f) * 255.f);
			}
		}
	}

	void c_colorpicker::apply_color(const hue::c_color& color)
	{
		*m_val = color;
		this->rgb_to_hsv();
	}

	void c_colorpicker::draw_context_menu()
	{
		if (!m_context_open && m_context_menu_anim.m_id == 0)
			return;

		m_context_menu_anim = utils::builder::create_animation_ctx(m_parent + m_label + "#context_menu", m_context_open && g_ctx->m_open, 0.55f);
		if (m_context_menu_anim.val() <= 0.f)
			return;

		const math::c_vector_2d menu_size = { 70.f, 75.f };
		const float row_h = 23.f;
		const float eased = smoothstep(m_context_menu_anim.val());
		const float alpha = animations::m_window_opacity.val();
		const math::c_vector_2d menu_pos = m_context_pos;
		const float origin_x = menu_pos.x;
		const float origin_y = menu_pos.y;
		math::c_rect menu_rect(menu_pos, menu_size);

		ImDrawList* dl = ImGui::GetForegroundDrawList();
		const int vtx_start = dl->VtxBuffer.Size;

		g_render->use_layer(engine::render_layer::prioritized, [&]()
		{
			g_render->rect_shadow(menu_pos.x, menu_pos.y, menu_size.x, menu_size.y, g_style->m_window_shadow.modulate(alpha * 0.45f), 10.f, 5.f);
			g_render->rect_filled(menu_pos.x, menu_pos.y, menu_size.x, menu_size.y, g_style->m_window_background.modulate(alpha), 5.f);

			const char* labels[] = { "Copy", "Paste", "Reset" };
			for (int i = 0; i < 3; i++)
			{
				const float item_y = menu_pos.y + 5.f + static_cast<float>(i) * row_h;
				math::c_rect item_rect({ menu_pos.x + 5.f, item_y }, { menu_size.x - 10.f, row_h - 3.f });
				const bool hovered = m_context_open && g_input->mouse_in_region(item_rect.pos(), item_rect.size());

				if (hovered)
					g_render->rect_filled(item_rect.x, item_rect.y, item_rect.w, item_rect.h, g_style->m_element_base.modulate(alpha * 0.8f), 4.f);

				g_font->f_childs.text(item_rect.x + 6.f, item_rect.y + 2.5f, labels[i], g_style->m_text.modulate(alpha * (hovered ? 0.75f : 0.45f)));
			}
		}, true);

		const int vtx_end = dl->VtxBuffer.Size;
		ImDrawVert* verts = dl->VtxBuffer.Data;
		for (int v = vtx_start; v < vtx_end; v++)
		{
			verts[v].pos.x = origin_x + (verts[v].pos.x - origin_x) * eased;
			verts[v].pos.y = origin_y + (verts[v].pos.y - origin_y) * eased;

			const int a = static_cast<int>(((verts[v].col >> IM_COL32_A_SHIFT) & 0xFF) * eased);
			verts[v].col = (verts[v].col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
		}

		if (m_context_open && g_input->clicked(input::mouse_buttons::left))
		{
			bool handled = false;
			for (int i = 0; i < 3; i++)
			{
				const float item_y = menu_pos.y + 5.f + static_cast<float>(i) * row_h;
				math::c_rect item_rect({ menu_pos.x + 5.f, item_y }, { menu_size.x - 10.f, row_h - 3.f });
				if (!g_input->mouse_in_region(item_rect.pos(), item_rect.size()))
					continue;

				if (i == 0)
				{
					g_colorpicker_clipboard = *m_val;
					g_colorpicker_clipboard_valid = true;
				}
				else if (i == 1)
				{
					hue::c_color pasted{};
					if (g_colorpicker_clipboard_valid)
						pasted = g_colorpicker_clipboard;
					else
						break;

					this->apply_color(pasted);
				}
				else
				{
					this->apply_color(m_default_val);
				}

				handled = true;
				m_context_open = false;
				close_context_menu(this, true);
				break;
			}

			if (!handled && !g_input->mouse_in_region(menu_rect.pos(), menu_rect.size()))
			{
				m_context_open = false;
				close_context_menu(this, false);
			}
		}

		if (m_context_open && !m_context_just_opened && g_input->clicked(input::mouse_buttons::right) && !g_input->mouse_in_region(menu_rect.pos(), menu_rect.size()))
		{
			m_context_open = false;
			close_context_menu(this, true);
		}

		m_context_just_opened = false;
	}

	void c_colorpicker::rgb_to_hsv()
	{
		if (!m_val)
			return;

		float r = m_val->r / 255.f;
		float g = m_val->g / 255.f;
		float b = m_val->b / 255.f;

		float max_val = std::max({ r, g, b });
		float min_val = std::min({ r, g, b });
		float delta = max_val - min_val;

		m_value = max_val;
		if (max_val == 0.f) {
			m_saturation = 0.f;
		}
		else {
			m_saturation = delta / max_val;
		}

		if (delta == 0.f) {
			m_hue = 0.f;
		}
		else if (max_val == r) {
			m_hue = 60.f * fmod(((g - b) / delta), 6.f);
		}
		else if (max_val == g) {
			m_hue = 60.f * (((b - r) / delta) + 2.f);
		}
		else {
			m_hue = 60.f * (((r - g) / delta) + 4.f);
		}

		if (m_hue < 0.f) m_hue += 360.f;
	}

	void c_colorpicker::hsv_to_rgb()
	{
		if (!m_val)
			return;

		float c = m_value * m_saturation;
		float x = c * (1.f - std::abs(fmod(m_hue / 60.f, 2.f) - 1.f));
		float m = m_value - c;

		float r, g, b;
		if (m_hue < 60.f) { r = c; g = x; b = 0; }
		else if (m_hue < 120.f) { r = x; g = c; b = 0; }
		else if (m_hue < 180.f) { r = 0; g = c; b = x; }
		else if (m_hue < 240.f) { r = 0; g = x; b = c; }
		else if (m_hue < 300.f) { r = x; g = 0; b = c; }
		else { r = c; g = 0; b = x; }

		this->m_val->r = (int)((r + m) * 255.f);
		this->m_val->g = (int)((g + m) * 255.f);
		this->m_val->b = (int)((b + m) * 255.f);
	}
}
