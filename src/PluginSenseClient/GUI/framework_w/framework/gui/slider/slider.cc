#include "../../../includes.hh"

namespace framework
{
	namespace
	{
		bool is_valid_numeric_buffer(const std::string& buffer, int precision)
		{
			int dot_count = 0;
			int decimal_count = 0;
			bool after_dot = false;

			for (int i = 0; i < static_cast<int>(buffer.size()); i++)
			{
				const char c = buffer[i];

				if (c >= '0' && c <= '9')
				{
					if (after_dot)
						decimal_count++;
					continue;
				}

				if (c == '-')
				{
					if (i != 0)
						return false;
					continue;
				}

				if (c == '.')
				{
					if (precision <= 0 || ++dot_count > 1)
						return false;
					after_dot = true;
					continue;
				}

				return false;
			}

			return decimal_count <= precision;
		}

		void insert_numeric_input(std::string& buffer, int& cursor, int precision)
		{
			cursor = std::clamp(cursor, 0, static_cast<int>(buffer.size()));

			for (ImWchar c : ImGui::GetIO().InputQueueCharacters)
			{
				if (c > 127)
					continue;

				std::string next = buffer;
				next.insert(next.begin() + cursor, static_cast<char>(c));

				if (!is_valid_numeric_buffer(next, precision))
					continue;

				buffer = next;
				cursor++;
			}
		}

		float cursor_x_for_index(const std::string& text, int cursor, float text_x)
		{
			cursor = std::clamp(cursor, 0, static_cast<int>(text.size()));
			return text_x + g_font->f_childs.measure(text.substr(0, cursor)).x;
		}
	}

	c_slider_float::c_slider_float(std::string label, float* val, float min, float max, bool hide_label, std::wstring prefix, int precision) : m_val(val), m_min(min), m_max(max), m_prefix(prefix), m_nPrecision(precision)
	{
		m_label = std::move(label);
		m_hide_label = hide_label;

		m_size = { 0, (m_hide_label ? 0 : g_font->f_childs.measure(m_label).y) + 15 };
		m_type = element_type::slider;
		m_focus_priority = focus_priority::persistent;

		// we only use this in terms of inlining elements
		m_parent_width = m_child_size;
	}

	void c_slider_float::input()
	{
		auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);
		math::c_rect bounding = math::c_rect(m_pos + math::c_vector_2d(0, position.y), math::c_vector_2d(m_child_size, 16.f));
		std::string info = utils::builder::precision(*this->m_val, m_nPrecision) + " " + utils::builder::wstring_to_string(this->m_prefix.c_str());
		const auto info_size = g_font->f_childs.measure(info);
		math::c_rect value_bounding = math::c_rect(m_pos + math::c_vector_2d(m_child_size - info_size.x - 4.f, -1.f), math::c_vector_2d(info_size.x + 8.f, info_size.y + 3.f));

		if (!g_ctx->can_interact(this, m_focus_priority))
			return;

		if (m_typing_value)
		{
			m_input_cursor = std::clamp(m_input_cursor, 0, static_cast<int>(m_input_buffer.size()));

			if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && m_input_cursor > 0)
				m_input_cursor--;

			if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && m_input_cursor < static_cast<int>(m_input_buffer.size()))
				m_input_cursor++;

			if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && m_input_cursor > 0)
			{
				m_input_buffer.erase(m_input_buffer.begin() + m_input_cursor - 1);
				m_input_cursor--;
			}

			insert_numeric_input(m_input_buffer, m_input_cursor, m_nPrecision);

			const bool confirm = ImGui::IsKeyPressed(ImGuiKey_Enter);
			const bool cancel = ImGui::IsKeyPressed(ImGuiKey_Escape);
			const bool clicked_outside = g_input->clicked(input::mouse_buttons::left) && !g_input->mouse_in_region(value_bounding.pos(), value_bounding.size());

			if (confirm || clicked_outside)
			{
				try
				{
					float value = std::clamp(std::stof(m_input_buffer), this->m_min, this->m_max);
					if (std::abs(value) < 0.000001f)
						value = 0.f;
					*this->m_val = value;
				}
				catch (...)
				{
				}

				m_typing_value = false;
				g_ctx->pop_focus(this);
				if (clicked_outside)
					g_ctx->m_click_consumed = true;
				return;
			}

			if (cancel)
			{
				m_typing_value = false;
				g_ctx->pop_focus(this);
				return;
			}

			return;
		}

		// check if hovered is nullptr and then if we are in region
		if (g_input->mouse_in_region(bounding.pos(), bounding.size()) || g_input->mouse_in_region(value_bounding.pos(), value_bounding.size()))
			g_ctx->m_hovered = this;
		else if (g_ctx->m_hovered == this)
		{
			g_ctx->m_hovered = nullptr;
		}

		if (!m_hide_label && ImGui::GetIO().KeyShift && g_input->clicked(input::mouse_buttons::left) && g_input->mouse_in_region(value_bounding.pos(), value_bounding.size()) && !g_ctx->m_click_consumed)
		{
			m_input_buffer = utils::builder::precision(*this->m_val, m_nPrecision);
			m_input_cursor = static_cast<int>(m_input_buffer.size());
			m_typing_value = true;
			g_ctx->push_focus(this, m_focus_priority);
			g_ctx->m_click_consumed = true;
			return;
		}

		if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this && !g_ctx->m_click_consumed)
		{
			g_ctx->push_focus(this, m_focus_priority);
			g_ctx->m_click_consumed = true;
		}

		if (g_input->click_down(input::mouse_buttons::left)) {
			if (g_ctx->is_focused(this))
			{
				float offset = std::clamp<float>(math::c_vector_2d(g_input->get_mouse_position() - this->m_pos).x, 0.f, m_child_size);
				float target_value = utils::builder::modulate_float(offset, 0.f, m_child_size, static_cast<float>(this->m_min), static_cast<float>(this->m_max));

				/* update value */
				const float current_value = *this->m_val;
				*this->m_val = current_value + (target_value - current_value) * 0.2f;
			}
		}

		// release focus on mouse up
		if (g_input->click_released(input::mouse_buttons::left) && g_ctx->is_focused(this))
			g_ctx->pop_focus(this);

	}

	void c_slider_float::draw()
	{
		animations::m_slider_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5f);
		animations::m_slider_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_slider_value", m_visible && (*this->m_val > this->m_min + 0.1f) && g_ctx->m_open, 0.5f);
		animations::m_slider_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_slider_hover", m_visible && g_ctx->m_hovered == this, 0.5f);

		// animation handling
		float target_opacity = 0.2f;
		if (animations::m_slider_value.val() > 0.f)
			target_opacity = 0.2f + (0.6f * animations::m_slider_value.val());
		else if (animations::m_slider_hover.val() > 0.f)
			target_opacity = 0.2f + (0.2f * animations::m_slider_hover.val());

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];

		float lerp_speed = 0.3f;
		smooth_opacity += (target_opacity - smooth_opacity) * lerp_speed;
		float final_opacity = animations::m_slider_opacity.val() * smooth_opacity;

		auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);
		auto position2 = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 7);

		g_render->use_layer(m_layer, [&]()
			{
				if (!m_hide_label)
					g_font->f_childs.text(m_pos.x, m_pos.y - 0.5f, m_label, g_style->m_text.modulate(final_opacity));

				g_render->rect_shadow((m_pos + position).x, (m_pos + position).y, m_child_size, 6.f, g_style->m_window_shadow.modulate(animations::m_window_opacity.limit(0.3f).val()), 8.f, 2.f);
				animations::m_window_opacity.restore();
				g_render->rect_filled((m_pos + position).x, (m_pos + position).y, m_child_size, 6.f, g_style->m_element_base.modulate(animations::m_window_opacity.val()), 2.f);

				float target_width = std::clamp(utils::builder::modulate_float(static_cast<float>(*this->m_val), static_cast<float>(this->m_min), static_cast<float>(this->m_max), 0.f, m_child_size), 0.f, m_child_size);

				float dt = ImGui::GetIO().DeltaTime;
				float speed = 10.f;

				m_display_width = target_width;
				if (m_display_width < 0.5f && target_width <= 0.f)
					m_display_width = 0.f;

				bool focused = g_ctx->is_focused(this);
				m_focus_anim += ((focused ? 1.f : 0.f) - m_focus_anim) * dt * speed;
				m_focus_anim = std::clamp(m_focus_anim, 0.f, 1.f);

				float eased = m_focus_anim * m_focus_anim * (3.f - 2.f * m_focus_anim);

				float scale = 1.f + eased * 0.3f;

				if (m_display_width > 0.5f)
				{
					auto schizo = m_pos + math::c_vector_2d(1, position.y);
					float center_y = schizo.y + 6.f * 0.5f;

					int vtx_start = g_render->draw_list()->VtxBuffer.Size;

					g_render->rect_shadow(schizo.x, schizo.y, m_display_width, 6.f,g_style->m_accent.modulate(animations::m_window_opacity.limit(0.4f).val()), 8.f, 2.f * scale);
					animations::m_window_opacity.restore();
					g_render->rect_filled(schizo.x, schizo.y, m_display_width, 6.f,g_style->m_accent.modulate(animations::m_window_opacity.val()), 2.f * scale);
					g_render->fade_rect_filled(schizo.x, schizo.y, m_display_width, 6.f,hue::c_color(0, 0, 0, 0),hue::c_color(0, 0, 0, (int)(50 * animations::m_window_opacity.val())),engine::fade_direction::vertically, 2.f * scale);

					int vtx_end = g_render->draw_list()->VtxBuffer.Size;

					float track_top = schizo.y - 4.f; 
					float track_bot = schizo.y + 8.f + 4.f;

					ImDrawVert* verts = g_render->draw_list()->VtxBuffer.Data;
					for (int v = vtx_start; v < vtx_end; v++)
					{
						verts[v].pos.y = center_y + (verts[v].pos.y - center_y) * scale;
						verts[v].pos.y = std::clamp(verts[v].pos.y, track_top, track_bot);
					}
				}

				std::string info = (m_typing_value ? m_input_buffer : utils::builder::precision(*this->m_val, m_nPrecision)) + " " + utils::builder::wstring_to_string(this->m_prefix.c_str());
				auto info_size = g_font->f_childs.measure(info);

				if (!m_hide_label)
				{
					const auto text_pos = m_pos + math::c_vector_2d(m_child_size - info_size.x, 0.5f);
					if (m_typing_value)
						g_render->rect_filled(text_pos.x - 4.f, text_pos.y - 1.f, info_size.x + 8.f, info_size.y + 3.f, g_style->m_element_base.modulate(animations::m_window_opacity.limit(0.45f).val()), 3.f);

					g_font->f_childs.text(text_pos.x, text_pos.y, info, g_style->m_text.modulate(animations::m_window_opacity.limit(m_typing_value ? 0.75f : 0.3f).val()));

					if (m_typing_value && (GetTickCount64() % 800) < 400)
					{
						const float cursor_x = cursor_x_for_index(m_input_buffer, m_input_cursor, text_pos.x);
						const float cursor_h = g_font->f_childs.measure("0").y - 2.f;
						g_render->rect_filled(cursor_x, text_pos.y + 2.f, 1.f, cursor_h, g_style->m_accent.modulate(animations::m_window_opacity.limit(0.8f).val()), 0.f);
					}
				}
				animations::m_window_opacity.restore();
			});
	}

	c_slider_int::c_slider_int(std::string label, int* val, int min, int max, bool hide_label, std::wstring prefix) : m_val(val), m_min(min), m_max(max), m_prefix(prefix)
	{
		m_label = std::move(label);
		m_hide_label = hide_label;

		m_size = { 0, (m_hide_label ? 0 : g_font->f_childs.measure(m_label).y) + 15 };
		m_type = element_type::slider;

		// we only use this in terms of inlining elements
		m_parent_width = m_child_size;
	}

	void c_slider_int::input()
	{
		auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);
		math::c_rect bounding = math::c_rect(m_pos + math::c_vector_2d(0, position.y), math::c_vector_2d(m_child_size, 16.f));
		std::string info = std::to_string(*this->m_val) + " " + utils::builder::wstring_to_string(this->m_prefix.c_str());
		const auto info_size = g_font->f_childs.measure(info);
		math::c_rect value_bounding = math::c_rect(m_pos + math::c_vector_2d(m_child_size - info_size.x - 4.f, -1.f), math::c_vector_2d(info_size.x + 8.f, info_size.y + 3.f));

		if (!g_ctx->can_interact(this, m_focus_priority))
			return;

		if (m_typing_value)
		{
			m_input_cursor = std::clamp(m_input_cursor, 0, static_cast<int>(m_input_buffer.size()));

			if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && m_input_cursor > 0)
				m_input_cursor--;

			if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && m_input_cursor < static_cast<int>(m_input_buffer.size()))
				m_input_cursor++;

			if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && m_input_cursor > 0)
			{
				m_input_buffer.erase(m_input_buffer.begin() + m_input_cursor - 1);
				m_input_cursor--;
			}

			insert_numeric_input(m_input_buffer, m_input_cursor, 0);

			const bool confirm = ImGui::IsKeyPressed(ImGuiKey_Enter);
			const bool cancel = ImGui::IsKeyPressed(ImGuiKey_Escape);
			const bool clicked_outside = g_input->clicked(input::mouse_buttons::left) && !g_input->mouse_in_region(value_bounding.pos(), value_bounding.size());

			if (confirm || clicked_outside)
			{
				try
				{
					int value = std::clamp(std::stoi(m_input_buffer), this->m_min, this->m_max);
					if (value == 0)
						value = 0;
					*this->m_val = value;
				}
				catch (...)
				{
				}

				m_typing_value = false;
				g_ctx->pop_focus(this);
				if (clicked_outside)
					g_ctx->m_click_consumed = true;
				return;
			}

			if (cancel)
			{
				m_typing_value = false;
				g_ctx->pop_focus(this);
				return;
			}

			return;
		}

		if (g_input->mouse_in_region(bounding.pos(), bounding.size()) || g_input->mouse_in_region(value_bounding.pos(), value_bounding.size()))
		{
			g_ctx->m_hovered = this;
		}
		else if (g_ctx->m_hovered == this)
		{
			g_ctx->m_hovered = nullptr;
		}

		if (!m_hide_label && ImGui::GetIO().KeyShift && g_input->clicked(input::mouse_buttons::left) && g_input->mouse_in_region(value_bounding.pos(), value_bounding.size()) && !g_ctx->m_click_consumed)
		{
			m_input_buffer = std::to_string(*this->m_val);
			m_input_cursor = static_cast<int>(m_input_buffer.size());
			m_typing_value = true;
			g_ctx->push_focus(this, m_focus_priority);
			g_ctx->m_click_consumed = true;
			return;
		}

		if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this && !g_ctx->m_click_consumed)
		{
			m_drag_value = (float)*this->m_val;
			g_ctx->push_focus(this, m_focus_priority);
			g_ctx->m_click_consumed = true;
		}

		if (g_input->click_down(input::mouse_buttons::left)) {
			if (g_ctx->is_focused(this))
			{
				float offset = std::clamp<float>(math::c_vector_2d(g_input->get_mouse_position() - this->m_pos).x, 0, m_child_size);

				/* we are forcing the conversion to float here using (float) */
				float target_value = utils::builder::modulate_float(offset, 0, m_child_size, (float)this->m_min, (float)this->m_max);

				m_drag_value += (target_value - m_drag_value) * 0.2f;
				*this->m_val = std::clamp((int)(m_drag_value + 0.5f), this->m_min, this->m_max);
			}
		}


		// release focus on mouse up
		if (g_input->click_released(input::mouse_buttons::left) && g_ctx->is_focused(this))
			g_ctx->pop_focus(this);
	}

	void c_slider_int::draw()
	{
		animations::m_slider_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5f);
		animations::m_slider_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_slider_int_value", m_visible && (*this->m_val > this->m_min) && g_ctx->m_open, 0.5f);
		animations::m_slider_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_slider_int_hover", m_visible && g_ctx->m_hovered == this, 0.5f);

		// animation handling
		float target_opacity = 0.2f;
		if (animations::m_slider_value.val() > 0.f)
			target_opacity = 0.2f + (0.6f * animations::m_slider_value.val());
		else if (animations::m_slider_hover.val() > 0.f)
			target_opacity = 0.2f + (0.2f * animations::m_slider_hover.val());

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];

		float lerp_speed = 0.3f;
		smooth_opacity += (target_opacity - smooth_opacity) * lerp_speed;
		float final_opacity = animations::m_slider_opacity.val() * smooth_opacity;

		auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);
		auto position2 = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 7);

		g_render->use_layer(m_layer, [&]()
			{
				if (!m_hide_label)
					g_font->f_childs.text(m_pos.x, m_pos.y - 0.5f, m_label, g_style->m_text.modulate(final_opacity));

				g_render->rect_shadow((m_pos + position).x, (m_pos + position).y, m_child_size, 6.f, g_style->m_window_shadow.modulate(animations::m_window_opacity.limit(0.3f).val()), 8.f, 2.f);
				animations::m_window_opacity.restore();
				g_render->rect_filled((m_pos + position).x, (m_pos + position).y, m_child_size, 6.f, g_style->m_element_base.modulate(animations::m_window_opacity.val()), 2.f);

				float target_width = std::clamp(utils::builder::modulate_float(static_cast<float>(*this->m_val), static_cast<float>(this->m_min), static_cast<float>(this->m_max), 0.f, m_child_size), 0.f, m_child_size);

				float dt = ImGui::GetIO().DeltaTime;
				float speed = 10.f;

				m_display_width = target_width;
				if (m_display_width < 0.5f && target_width <= 0.f)
					m_display_width = 0.f;

				bool focused = g_ctx->is_focused(this);
				m_focus_anim += ((focused ? 1.f : 0.f) - m_focus_anim) * dt * speed;
				m_focus_anim = std::clamp(m_focus_anim, 0.f, 1.f);

				float eased = m_focus_anim * m_focus_anim * (3.f - 2.f * m_focus_anim);

				float scale = 1.f + eased * 0.3f;

				if (m_display_width > 0.5f)
				{
					auto schizo = m_pos + math::c_vector_2d(1, position.y);
					float center_y = schizo.y + 6.f * 0.5f;

					int vtx_start = g_render->draw_list()->VtxBuffer.Size;

					g_render->rect_shadow(schizo.x, schizo.y, m_display_width, 6.f, g_style->m_accent.modulate(animations::m_window_opacity.limit(0.4f).val()), 8.f, 2.f * scale);
					animations::m_window_opacity.restore();
					g_render->rect_filled(schizo.x, schizo.y, m_display_width, 6.f, g_style->m_accent.modulate(animations::m_window_opacity.val()), 2.f * scale);
					g_render->fade_rect_filled(schizo.x, schizo.y, m_display_width, 6.f, hue::c_color(0, 0, 0, 0), hue::c_color(0, 0, 0, (int)(50 * animations::m_window_opacity.val())), engine::fade_direction::vertically, 2.f * scale);

					int vtx_end = g_render->draw_list()->VtxBuffer.Size;

					float track_top = schizo.y - 4.f;
					float track_bot = schizo.y + 8.f + 4.f;

					ImDrawVert* verts = g_render->draw_list()->VtxBuffer.Data;
					for (int v = vtx_start; v < vtx_end; v++)
					{
						verts[v].pos.y = center_y + (verts[v].pos.y - center_y) * scale;
						verts[v].pos.y = std::clamp(verts[v].pos.y, track_top, track_bot);
					}
				}

				std::string info = (m_typing_value ? m_input_buffer : std::to_string(*this->m_val)) + " " + utils::builder::wstring_to_string(this->m_prefix.c_str());
				auto info_size = g_font->f_childs.measure(info);

				if (!m_hide_label)
				{
					const auto text_pos = m_pos + math::c_vector_2d(m_child_size - info_size.x, 0.5f);
					if (m_typing_value)
						g_render->rect_filled(text_pos.x - 4.f, text_pos.y - 1.f, info_size.x + 8.f, info_size.y + 3.f, g_style->m_element_base.modulate(animations::m_window_opacity.limit(0.45f).val()), 3.f);

					g_font->f_childs.text(text_pos.x, text_pos.y, info, g_style->m_text.modulate(animations::m_window_opacity.limit(m_typing_value ? 0.75f : 0.3f).val()));

					if (m_typing_value && (GetTickCount64() % 800) < 400)
					{
						const float cursor_x = cursor_x_for_index(m_input_buffer, m_input_cursor, text_pos.x);
						const float cursor_h = g_font->f_childs.measure("0").y - 2.f;
						g_render->rect_filled(cursor_x, text_pos.y + 2.f, 1.f, cursor_h, g_style->m_accent.modulate(animations::m_window_opacity.limit(0.8f).val()), 0.f);
					}
				}
				animations::m_window_opacity.restore();
			});
	}
}
