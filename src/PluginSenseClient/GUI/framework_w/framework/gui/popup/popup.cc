#include "../../../includes.hh"

namespace framework
{
	c_popup::c_popup(std::string label, bool hide_label)
	{
		m_label = std::move(label);
		m_hide_label = hide_label;
		m_size = { 0, (m_hide_label ? 0.f : 16.f) };

		m_type = element_type::popup;
		m_focus_priority = focus_priority::modal;

		// we only use this in terms of inlining elements
		m_parent_width = 20;
	}

	void c_popup::draw()
	{
		const std::string popup_id = m_parent + m_label + "#popup";
		animations::m_popup_opacity = utils::builder::create_animation_ctx(popup_id, m_visible && g_ctx->m_open, 0.5f);
		animations::m_popup_value = utils::builder::create_animation_ctx(popup_id + "#m_popup_value", m_visible && g_ctx->top_focus() == this && g_ctx->m_open, 0.5f);
		animations::m_popup_hover = utils::builder::create_animation_ctx(popup_id + "#m_popup_hover", m_visible && g_ctx->m_hovered == this, 0.5f);

		// animation handling
		float target_opacity = 0.2f;
		if (animations::m_popup_value.val() > 0.f)
			target_opacity = 0.2f + (0.6f * animations::m_popup_value.val());
		else if (animations::m_popup_hover.val() > 0.f)
			target_opacity = 0.2f + (0.2f * animations::m_popup_hover.val());

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];

		float lerp_speed = 0.3f;
		smooth_opacity += (target_opacity - smooth_opacity) * lerp_speed;
		float final_opacity = animations::m_popup_opacity.val() * smooth_opacity;

		if (!m_headless)
		{
			if (!m_hide_label)
			{
				// data when hide label is false its diff, if we're gonna inline elements atleast do it properly
				g_font->f_childs.text(m_pos.x, m_pos.y - 0.5f, m_label, g_style->m_text.modulate(final_opacity));
				g_font->f_icons.text(m_pos.x + (m_child_size - g_font->f_icons.measure(ICON_FA_GEAR).x) - 1, m_pos.y, ICON_FA_GEAR, g_style->m_text.modulate(final_opacity));
			}
			else
			{
				// imagine if we have multiinline
				g_font->f_icons.text(m_pos.x, m_pos.y, ICON_FA_GEAR, g_style->m_text.modulate(final_opacity));
			}
		}

		if (animations::m_popup_value.val() > 0.f)
		{
			// note: the popup will have a base size that will increase over elements
			math::c_vector_2d pallete_size = { 190, this->m_height };

			// position based on the label status
			math::c_vector_2d pallete_pos = !m_hide_label
				? math::c_vector_2d(m_pos.x + m_child_size - (g_font->f_icons.measure(ICON_FA_PALETTE).x), m_pos.y + 20)
				: math::c_vector_2d(m_pos.x, m_pos.y + 20);

			float padding = 0.f;
			c_base_element* m_parent_control = nullptr;

			float t = animations::m_popup_value.val();
			float eased = t * t * (3.f - 2.f * t);

			float origin_y = pallete_pos.y;
			float origin_x = pallete_pos.x;

			// match colorpicker/keybind popup layering
			ImDrawList* dl = g_render->draw_list();
			int vtx_start = dl->VtxBuffer.Size;

			g_render->use_layer(engine::render_layer::prioritized, [&]()
			{
				// draw the background of the pallete
				g_render->rect_shadow(pallete_pos.x, pallete_pos.y, pallete_size.x, pallete_size.y, g_style->m_window_shadow.modulate(animations::m_popup_value.val()), 10.f, 5.f);
				g_render->rect_filled(pallete_pos.x, pallete_pos.y, pallete_size.x, pallete_size.y, g_style->m_window_background.modulate(animations::m_popup_value.val()), 5.f);

				if (!m_hide_popup_title)
				{
					g_render->rect_filled(pallete_pos.x, pallete_pos.y, pallete_size.x, 25.f, g_style->m_window_bars.modulate(animations::m_popup_value.val()), 5.f);
					g_render->gradient(pallete_pos.x, pallete_pos.y + 25.f, pallete_size.x, 10, g_style->m_window_shadow.modulate(animations::m_popup_value.limit(0.2f).val()), g_style->m_window_shadow.modulate(animations::m_popup_value.limit(0.1f).val()).with_alpha(0), engine::fade_direction::horizontally);

					// pallete name
					g_font->f_childs.text(pallete_pos.x + 8, pallete_pos.y + 3, m_label, g_style->m_text.modulate(animations::m_popup_value.limit(0.6f).val()));
					animations::m_popup_value.restore();
				}

				//g_render->rect(pallete_pos.x - 1, pallete_pos.y - 1, pallete_size.x + 2, pallete_size.y + 2, g_style->m_outline.modulate(animations::m_popup_value.val()), 4.f);	

				// some kind dogshit ass code here
				auto calculate_element_padding = [&]() -> math::c_vector_2d
					{
						return math::c_vector_2d(12.f, m_hide_popup_title ? 12.f : 35.f);
					};

				auto calculate_safe_area = [&]() -> math::c_vector_2d
					{
						return pallete_size - math::c_vector_2d(18.f, m_hide_popup_title ? 18.f : 40.f);
					};

				float height = 37.f;

				// we can now do the element setup like initializatioon, position and other doshits
				for (auto control : this->m_controls)
				{
					// override layer
					control->set_layer(engine::render_layer::prioritized);

					// we do not want inlined controls in popup so we can just disable this for now
					if (control->m_inlined)
					{
						if (control->m_type == element_type::colorpicker)
						{
							// lets check if the parent is visible
							if (m_parent_control->m_callback_visibility)
							{
								control->m_callback_visibility = true;
								control->m_visible_by_callback = m_parent_control->m_visible_by_callback;
							}

							// if the colorpicker is inlined disabled label
							control->hide_label();

							// if we are inlining to a checkbox we are setting parent + pos - icon
							if (m_parent_control->m_type == element_type::checkbox)
							{
								control->m_pos = m_parent_control->m_pos + math::c_vector_2d((control->m_child_size - g_font->f_icons.measure(ICON_FA_PALETTE).x) - 2, 0);
							} // down from here is multiinlining
							else if (m_parent_control->m_type == element_type::colorpicker)
							{
								control->m_pos = m_parent_control->m_pos - math::c_vector_2d(m_parent_control->m_parent_width + 5.f, 0);
							}
						}
						else
						{
							control->m_inlined = false;
							continue; // we might not need this
						}
					}
					else
					{
						control->m_pos = pallete_pos + math::c_vector_2d(0, padding) + calculate_element_padding();
					}

					// forget about drawing
					if (animations::m_popup_value.val() <= 0.f)
					{
						continue;
					}

					if (control->m_callback_visibility && control->m_visible_by_callback && !control->m_visible_by_callback())
						continue;

					// c_base_control::base
					// set the child parent, we are going to use this in the checkbox data ( if there are problems, make sure to include, tab subtab )
					control->set_parent(this->m_label + "#");
					control->set_visibility(true);

					// c_base_control->element
					// no point in inputting if we have no menu opened
					if (g_ctx->m_open) {
						control->input();
					}

					if (control->m_call_stacks)
						control->m_call_stacks();

					control->draw();


					// no need for inline check
					if (!control->m_inlined)
						padding += control->m_size.y + 8.f;

					// we only set this if we do inlining
					control->m_parent_control = m_parent_control;
					m_parent_control = control.get();

					// set child data to control base
					control->m_child_size = pallete_size.x - 25.f;

					//height += padding;
					//if (g_ctx->m_focused)
					//	g_ctx->m_focus_took = control.get();
				}
				this->m_height = padding + (m_hide_popup_title ? 20.f : 42.f);
			});

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
			if (!g_input->mouse_in_region(bounding_data.pos(), bounding_data.size())
				&& g_input->clicked(input::mouse_buttons::left)
				&& !g_ctx->m_click_consumed)
			{
				// check if any inner control is focused — if so, don't close
				bool inner_focused = false;
				for (auto& control : this->m_controls) {
					if (g_ctx->is_focused(control.get())) {
						inner_focused = true;
						break;
					}
				}

				if (!inner_focused) {
					g_ctx->pop_focus(this);
					g_ctx->m_modal_owner = nullptr;
					g_ctx->m_click_consumed = true;
				}
			}
		}
	}

	void c_popup::input()
	{
		const auto icon_size = g_font->f_icons.measure(ICON_FA_GEAR);
		math::c_rect bounding = math::c_rect(m_pos, math::c_vector_2d(m_hide_label ? icon_size.x : m_child_size, m_hide_label ? icon_size.y : m_size.y));

		if (!g_ctx->can_interact(this, m_focus_priority))
			return;

		// another element is in focus, we dont want to trigger another one
		//if (g_ctx->m_focus_took != nullptr && g_ctx->m_focus_took != this)
		//	return;

		if (g_input->mouse_in_region(bounding.pos(), bounding.size()))
		{
			g_ctx->m_hovered = this;
		}
		else if (g_ctx->m_hovered == this) {
			g_ctx->m_hovered = nullptr;
		}

		if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this)
		{
			if (g_ctx->top_focus() == this)
			{
				g_ctx->pop_focus(this);
				g_ctx->m_modal_owner = nullptr;
				g_ctx->m_click_consumed = true;
				return;
			}

			g_ctx->push_focus(this, m_focus_priority);
			g_ctx->m_modal_owner = this;
			g_ctx->m_click_consumed = true;
			//g_ctx->m_focused = g_ctx->m_focus_took_by_popup = this; // we opened it mfucker
		}
	}

	std::shared_ptr<c_checkbox> c_popup::add_checkbox(std::string label, bool* val)
	{
		auto control = std::make_shared<framework::c_checkbox>(label, val);
		{
			// cache this new added element
			this->m_controls.push_back(control);

			slog::log::success("[framework::c_popup] created a new control: [type: c_checkbox, label: {}]", label);
		}

		// allow chaining (->colorpicker(), ->popup())
		return control;
	}

	std::shared_ptr<c_slider_float> c_popup::add_slider_float(std::string label, float* val, float min, float max, bool hide_label, std::wstring prefix)
	{
		auto control = std::make_shared<framework::c_slider_float>(label, val, min, max, hide_label, prefix);
		{
			// cache this new added element
			this->m_controls.push_back(control);

			slog::log::success("[framework::c_popup] created a new control: [type: c_slider_float, label: {}]", label);
		}

		// allow chaining (->colorpicker(), ->popup())
		return control;
	}

	std::shared_ptr<c_slider_int> c_popup::add_slider_int(std::string label, int* val, int min, int max, bool hide_label, std::wstring prefix)
	{
		auto control = std::make_shared<framework::c_slider_int>(label, val, min, max, hide_label, prefix);
		{
			// cache this new added element
			this->m_controls.push_back(control);

			slog::log::success("[framework::c_popup] created a new control: [type: c_slider_int, label: {}]", label);
		}

		// allow chaining (->colorpicker(), ->popup())
		return control;
	}

	std::shared_ptr<c_dropdown> c_popup::add_dropdown(std::string label, int* val, std::vector<std::string> items, bool hide_label)
	{
		auto control = std::make_shared<c_dropdown>(label, val, items, hide_label);
		{
			this->m_controls.push_back(control);

			slog::log::success("[framework::c_popup] created a new control: [type: c_dropdown, label: {}]", label);
		}

		// allow chaining
		return control;
	}

	std::shared_ptr<c_multidropdown> c_popup::add_multibox(std::string label, bool hide_label, std::function<void(c_multidropdown* ptr)> callback)
	{
		auto control = std::make_shared<c_multidropdown>(label, hide_label);
		{
			callback(control.get());

			this->m_controls.push_back(control);

			slog::log::success("[framework::c_popup] created a new control: [type: c_multidropdown, label: {}]", label);
		}

		return control;
	}

	std::shared_ptr<c_button> c_popup::add_button(std::string label, std::function<void()> callback)
	{
		auto control = std::make_shared<c_button>(label, callback);
		{
			this->m_controls.push_back(control);
			slog::log::success("[framework::c_popup] created a new control: [type: c_button, label: {}]", label);
		}

		return control;
	}

	std::shared_ptr<c_colorpicker> c_popup::add_colorpicker(std::string label, hue::c_color* val, bool hide_label)
	{
		auto control = std::make_shared<c_colorpicker>(label, val, hide_label);
		{
			this->m_controls.push_back(control);
			slog::log::success("[framework::c_popup] created a new control: [type: c_colorpicker, label: {}]", label);
		}

		return control;
	}

	std::shared_ptr<c_keybind> c_popup::add_keybind(std::string label, key_var_t* val, bool hide_label)
	{
		auto control = std::make_shared<c_keybind>(label, val, hide_label);
		{
			this->m_controls.push_back(control);
			slog::log::success("[framework::c_popup] created a new control: [type: c_keybind, label: {}]", label);
		}

		return control;
	}

	std::shared_ptr<c_text_input> c_popup::add_input_box(std::string label, std::string* val, bool hide_label)
	{
		auto control = std::make_shared<c_text_input>(label, val, hide_label);
		{
			this->m_controls.push_back(control);
			slog::log::success("[framework::c_popup] created a new control: [type: c_text_input, label: {}]", label);
		}
		return control;
	}
}
