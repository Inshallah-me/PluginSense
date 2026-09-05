#include "../../../includes.hh"

namespace framework
{
	c_child::c_child(std::string name, child_width width, float y) : m_name(std::move(name)), m_width_type(width), m_size(math::c_vector_2d(0, y)), m_titlebar(true) {}

	void c_child::draw()
	{
		// int vtx_start = g_render->draw_list()->VtxBuffer.Size;

		g_render->rect_shadow(this->m_pos.x, this->m_pos.y, this->m_size.x, this->m_size.y, hue::c_color(0, 0, 0).modulate(this->m_child_opacity.limit(0.3f).val()), 10.f, 7.f);
		this->m_child_opacity.restore();

		g_render->rect_filled(this->m_pos.x, this->m_pos.y, this->m_size.x, this->m_size.y, g_style->m_window_background.modulate(this->m_child_opacity.val()), 7);
		g_render->rect_filled(this->m_pos.x, this->m_pos.y, this->m_size.x, 35, g_style->m_window_bars.modulate(this->m_child_opacity.val()), 7);

		if (this->m_titlebar)
		{
			g_render->gradient(this->m_pos.x, this->m_pos.y + 35, this->m_size.x, 10, g_style->m_window_shadow.modulate(this->m_child_opacity.limit(0.2f).val()), g_style->m_window_shadow.modulate(this->m_child_opacity.limit(0.1f).val()).with_alpha(0), engine::fade_direction::horizontally);
			this->m_child_opacity.restore();
			g_font->f_default.text(this->m_pos.x + 10, this->m_pos.y + 7, this->m_name, g_style->m_text.modulate(this->m_child_opacity.limit(0.6f).val()));
		}
		this->m_child_opacity.restore();

		// element handling
		float padding = 0.f;
		c_base_element* m_parent_control = nullptr;

		math::c_vector_2d content_start = this->m_pos + this->calculate_element_padding();
		float content_area_height = this->m_size.y - this->calculate_element_padding().y;

		for (auto& control : m_controls)
		{
			if (fn && visible())
				fn();

			// if control is inlined we use parent's control data to init this one
			if (control->m_inlined && m_parent_control != nullptr) {
				// we have to check if the element is a slider since it needs abit more presuring on it
				if (control->m_type == element_type::slider)
				{
					// if its slider we also have to lower out the m_child_size
					float base_scalling = this->m_size.x - 20.f;

					// check if parent is slider
					if (m_parent_control->m_type == element_type::slider)
					{
						m_parent_control->m_child_size = m_parent_control->m_parent_width = (base_scalling * 0.5f) - 7.5f;
					}

					control->m_child_size = (base_scalling * 0.5f) - 7.5f;

					// hide label if the parent control is a checkbox
					if (m_parent_control->m_type == element_type::checkbox && !control->m_hide_label)
					{
						// force label hide
						control->hide_label();
					}

					// check if parent control is checkbox so we align it when we have no label
					if (m_parent_control->m_type == element_type::checkbox && control->m_hide_label)
					{
						control->m_pos = m_parent_control->m_pos + math::c_vector_2d(m_parent_control->m_parent_width + 15.f, 4);
					}
					else {
						control->m_pos = m_parent_control->m_pos + math::c_vector_2d(m_parent_control->m_parent_width + 15.f, 0);
					}
				}
				else if (control->m_type == element_type::button)
				{
					// check if parent is not a button, so we dont allow it
					if (m_parent_control->m_type != element_type::button)
					{
						// do not allow inlining
						control->m_inlined = false;
						continue;
					}

					// if its slider we also have to lower out the m_child_size
					float base_scalling = this->m_size.x - 20.f;

					m_parent_control->m_child_size = m_parent_control->m_parent_width = (base_scalling * 0.5f) - 7.5f;
					control->m_child_size = (base_scalling * 0.5f) - 7.5f;

					control->m_pos = m_parent_control->m_pos + math::c_vector_2d(m_parent_control->m_parent_width + 10.f, 0);
				}
				else if (control->m_type == element_type::text_input)
				{
					// check if parent is not a button, so we dont allow it
					if (m_parent_control->m_type != element_type::text_input)
					{
						// do not allow inlining
						control->m_inlined = false;
						continue;
					}

					// if its slider we also have to lower out the m_child_size
					float base_scalling = this->m_size.x - 20.f;

					m_parent_control->m_child_size = m_parent_control->m_parent_width = (base_scalling * 0.5f) - 7.5f;
					control->m_child_size = (base_scalling * 0.5f) - 7.5f;

					control->m_pos = m_parent_control->m_pos + math::c_vector_2d(m_parent_control->m_parent_width + 10.f, 0);
				}
				else if (control->m_type == element_type::colorpicker)
				{
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
					else if (m_parent_control->m_type == element_type::keybind)
					{
						control->m_pos = m_parent_control->m_pos - math::c_vector_2d(g_font->f_icons.measure(ICON_FA_PALETTE).x + 8.f, 0);
					}
				}
				else if (control->m_type == element_type::keybind)
				{
					// if the colorpicker is inlined disabled label
					control->hide_label();

					// if we are inlining to a checkbox we are setting parent + pos - icon
					if (m_parent_control->m_type == element_type::checkbox)
					{
						control->m_pos = m_parent_control->m_pos + math::c_vector_2d((control->m_child_size - g_font->f_icons.measure(ICON_FA_KEYBOARD).x) - 2, 0);
					} // down from here is multiinlining
					else if (m_parent_control->m_type == element_type::colorpicker)
					{
						control->m_pos = m_parent_control->m_pos - math::c_vector_2d(m_parent_control->m_parent_width + 8.f, 0);
					}
					else
					{
						// 其他父控件(如 multibox):图标推到该行最右侧,
						// 和普通 keybind 的图标位置一致(标签靠左、图标贴最右)
						const float icon_w = g_font->f_icons.measure(ICON_FA_KEYBOARD).x;
						control->m_pos = m_parent_control->m_pos + math::c_vector_2d(m_parent_control->m_child_size - icon_w - 2.f, 0);
					}
				}
				else if (control->m_type == element_type::popup)
				{
					// if the colorpicker is inlined disabled label
					control->hide_label();

					// if we are inlining to a checkbox we are setting parent + pos - icon
					if (m_parent_control->m_type == element_type::checkbox)
					{
						control->m_pos = m_parent_control->m_pos + math::c_vector_2d((control->m_child_size - g_font->f_icons.measure(ICON_FA_GEAR).x) - 2, 0);
					} // down from here is multiinlining
					else if (m_parent_control->m_type == element_type::colorpicker)
					{
						control->m_pos = m_parent_control->m_pos - math::c_vector_2d(m_parent_control->m_parent_width + 8.f, 0);
					}
				}
				else {
					control->m_pos = m_parent_control->m_pos + math::c_vector_2d(m_parent_control->m_parent_width + 15.f, 0);
				}
			}
			else {
				control->m_pos = this->m_pos + math::c_vector_2d(0, padding - m_scroll_offset) + this->calculate_element_padding();
			}

			if (!this->visible())
			{
				continue;
			}

			// callback visibility
			if (control->m_callback_visibility && control->m_visible_by_callback && !control->m_visible_by_callback())
				continue;

			float control_top = control->m_pos.y;
			float control_bottom = control->m_pos.y + control->m_size.y;
			float visible_top = content_start.y;
			float visible_bottom = this->m_pos.y + this->m_size.y - 12.f;

			bool is_visible = false;

			if (!g_search.m_should_draw)
				is_visible = !(control_bottom < visible_top || control_top > visible_bottom);
			else 
				is_visible = true;

			// c_base_control::base
			// set the child parent, we are going to use this in the checkbox data ( if there are problems, make sure to include, tab subtab )
			control->set_parent(this->m_name + "#" + this->m_child_attach_data.m_subtab_name);


			// set base control visibility based on where we are
			control->set_visibility(this->visible() && is_visible);

			// c_base_control->element
			// no point in inputting if we have no menu opened
			if (g_ctx->m_open && is_visible)
			{
				// if a modal is open, only allow the popup that owns it to run input
				// everything else behind it gets blocked
				if (g_ctx->m_modal_owner != nullptr && control.get() != g_ctx->m_modal_owner)
				{
					// skip input for background elements
				}
				else if (g_ctx->can_interact(control.get(), control->m_focus_priority))
				{
					if (!g_search.m_should_draw)
						control->input();
				}
			}

			// engine::c_layout_engine(this->m_pos + this->calculate_element_padding(), this->calculate_safe_area())
			if (control->m_type != element_type::popup && control->m_type != element_type::interactive_preview)
				g_render->push_clip((this->m_pos + this->calculate_element_padding()).x - 5.f, (this->m_pos + this->calculate_element_padding()).y - 5.f, this->calculate_safe_area().x, this->calculate_safe_area().y);

			if (is_visible)
			{
				if (control->m_call_stacks)
					control->m_call_stacks();

				control->draw();
			}

			if (control->m_type != element_type::popup && control->m_type != element_type::interactive_preview)
				g_render->restore_clip();

			// push y only if the control is not inlined
			if (!control->m_inlined)
				padding += control->m_size.y + 8.f;

			// we only set this if we do inlining
			control->m_parent_control = m_parent_control;
			m_parent_control = control.get();

			// set child data to control base
			control->m_child_size = this->m_size.x - 25.f;

			// g_ctx->m_focus_took = control.get();

			// check which control is focused
			if (g_ctx->m_focus_took != nullptr)
				slog::log::debug("[framework::c_child] control focused: {}", g_ctx->m_focus_took->m_label);

			animations::m_window_opacity.restore();
		}

		m_content_height = padding;
		m_max_scroll = std::max(0.f, m_content_height - content_area_height);

		if (this->visible() && m_max_scroll > 0.f)
		{
			const float track_h = this->calculate_safe_area().y;
			const float track_x = this->m_pos.x + this->m_size.x - 7.f;
			const float track_y = content_start.y - 2.f;
			const float thumb_h = std::clamp((track_h / std::max(m_content_height, track_h)) * track_h, 18.f, track_h);
			const float thumb_range = std::max(1.f, track_h - thumb_h);
			const float thumb_y = track_y + (m_scroll_offset / std::max(1.f, m_max_scroll)) * thumb_range;
			const float alpha = this->m_child_opacity.val();

			g_render->rect_filled(track_x, track_y, 2.f, track_h, hue::c_color(70, 70, 70).modulate(alpha * 0.35f), 2.f);
			g_render->rect_filled(track_x, thumb_y, 2.f, thumb_h, hue::c_color(125, 125, 125).modulate(alpha * 0.55f), 2.f);
		}

		// reset it
		animations::m_window_opacity.restore();

		//int vtx_end = g_render->draw_list()->VtxBuffer.Size;
		//
		//float opacity = this->m_child_opacity.val();
		//if (vtx_end > vtx_start)
		//{
		//	float center_y = this->m_pos.y + this->m_size.y * 0.5f;
		//	ImDrawVert* verts = g_render->draw_list()->VtxBuffer.Data;
		//
		//	for (int i = vtx_start; i < vtx_end; i++)
		//	{
		//		verts[i].pos.y = center_y + (verts[i].pos.y - center_y) * opacity;
		//
		//		ImU32 col = verts[i].col;
		//		int a = (col >> IM_COL32_A_SHIFT) & 0xFF;
		//		a = (int)(a * opacity);
		//		verts[i].col = (col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
		//	}
		//}

	}

	void c_child::input()
	{
		bool is_hovered = g_input->mouse_in_region(this->m_pos, this->m_size);
		if (is_hovered && this->visible() && m_max_scroll > 0.f && g_ctx->top_focus() == nullptr)
		{
			// 鼠标悬停在 listbox 的可滚动区域上时,滚动应只作用于该控件,
			// 不联动滚动整个分区(否则列表滚动时分区也跟着滚)
			// 用控件当前 m_pos(上一帧 draw 已设置)+ listbox 自身滚动区域精确判断
			bool overScrollable = false;
			for (const auto& control : m_controls)
			{
				if (!control || control->m_type != framework::element_type::listbox)
					continue;
				auto* lb = dynamic_cast<framework::c_listbox*>(control.get());
				if (lb && lb->is_mouse_over_scroll_area())
				{
					overScrollable = true;
					break;
				}
			}

			if (!overScrollable)
			{
				float scroll_speed = 40.f;
				float scroll_delta = g_input->get_wheel_value();

				m_scroll_target -= scroll_delta * scroll_speed;
			}
		}

		// 内容变短(删除列表项/隐藏控件)后 m_max_scroll 会缩小,
		// 这里每帧 clamp 一次,保证滚动不会停在越界位置(否则顶部控件被滚出视野)
		m_scroll_target = std::clamp(m_scroll_target, 0.f, m_max_scroll);

		float lerp_speed = 0.15f;
		m_scroll_offset += (m_scroll_target - m_scroll_offset) * lerp_speed;

		if (std::abs(m_scroll_target - m_scroll_offset) < 0.5f)
		{
			m_scroll_offset = m_scroll_target;
		}
	}

	void c_child::attach_child(std::string tab_name, std::string subtab_name, int tab_index)
	{
		this->m_child_attach_data.m_tab_name = tab_name;
		this->m_child_attach_data.m_subtab_name = subtab_name;
		this->m_child_attach_data.tab = tab_index;
	}

	void c_child::set_visible(bool data)
	{
		this->m_visible = data;
	}

	bool c_child::visible()
	{
		if (g_ctx->m_cur_tab != this->m_child_attach_data.m_tab_name)
		{
			return false;
		}

		bool same_tab = g_ctx->m_cur_tab == this->m_child_attach_data.m_tab_name;
		if (same_tab && g_ctx->m_tabs[g_ctx->m_active_tab].m_subtab.empty())
		{
			return true;
		}

		if (same_tab && g_ctx->m_tabs[g_ctx->m_active_tab].m_cur_subtab != this->m_child_attach_data.m_subtab_name)
		{
			return false;
		}

		if (same_tab && g_ctx->m_tabs[g_ctx->m_active_tab].m_cur_subtab == this->m_child_attach_data.m_subtab_name)
		{
			return true;
		}

		// if none of the conditions matched return false
		return false;
	}

	child_width c_child::get_type()
	{
		return this->m_width_type;
	}

	math::c_vector_2d c_child::calculate_element_padding()
	{
		if (this->m_titlebar)
		{
			return math::c_vector_2d(12.f, 45.f);
		}
		else {
			return math::c_vector_2d(10.f, 12.f);
		}
	}

	math::c_vector_2d c_child::calculate_safe_area()
	{
		if (this->m_titlebar)
		{
			return this->m_size - math::c_vector_2d(18.f, 50.f);
		}
		else {
			return this->m_size - math::c_vector_2d(20.f, 24.f);
		}
	}

	std::shared_ptr<framework::c_checkbox> c_child::add_checkbox(std::string label, bool* val)
	{
		auto control = std::make_shared<framework::c_checkbox>(label, val);
		{
			// cache this new added element
			this->m_controls.push_back(control);

			// just add the control

			auto copied = std::make_shared<c_checkbox>(*control);
			g_search.add_to_database(copied, this->m_child_attach_data.m_tab_name, this->m_child_attach_data.tab);

			slog::log::success("[framework::c_child] created a new control: [type: c_checkbox, label: {}]", label);
		}

		// allow chaining (->colorpicker(), ->popup())
		return control;
	}

	std::shared_ptr<c_slider_float> c_child::add_slider_float(std::string label, float* val, float min, float max, bool hide_label, std::wstring prefix, int precision)
	{
		auto control = std::make_shared<framework::c_slider_float>(label, val, min, max, hide_label, prefix, precision);
		{
			// cache this new added element
			this->m_controls.push_back(control);

			auto copied = std::make_shared<c_slider_float>(*control);
			g_search.add_to_database(copied, this->m_child_attach_data.m_tab_name, this->m_child_attach_data.tab);

			slog::log::success("[framework::c_child] created a new control: [type: c_slider_float, label: {}]", label);
		}

		// allow chaining (->colorpicker(), ->popup())
		return control;
	}

	std::shared_ptr<c_slider_int> c_child::add_slider_int(std::string label, int* val, int min, int max, bool hide_label, std::wstring prefix)
	{
		auto control = std::make_shared<framework::c_slider_int>(label, val, min, max, hide_label, prefix);
		{
			// cache this new added element
			this->m_controls.push_back(control);

			// just add the control
			auto copied = std::make_shared<c_slider_int>(*control);
			g_search.add_to_database(copied, this->m_child_attach_data.m_tab_name, this->m_child_attach_data.tab);

			slog::log::success("[framework::c_child] created a new control: [type: c_slider_int, label: {}]", label);
		}

		// allow chaining (->colorpicker(), ->popup())
		return control;
	}

	std::shared_ptr<c_dropdown> c_child::add_dropdown(std::string label, int* val, std::vector<std::string> items, bool hide_label)
	{
		auto control = std::make_shared<c_dropdown>(label, val, items, hide_label);
		{
			this->m_controls.push_back(control);

			// just add the control
			auto copied = std::make_shared<c_dropdown>(*control);
			g_search.add_to_database(copied, this->m_child_attach_data.m_tab_name, this->m_child_attach_data.tab);

			slog::log::success("[framework::c_child] created a new control: [type: c_dropdown, label: {}]", label);
		}

		// allow chaining
		return control;
	}

	std::shared_ptr<c_multidropdown> c_child::add_multibox(std::string label, bool hide_label, std::function<void(c_multidropdown* ptr)> callback)
	{
		auto control = std::make_shared<c_multidropdown>(label, hide_label);
		{
			callback(control.get());

			this->m_controls.push_back(control);

			// just add the control
			auto copied = std::make_shared<c_multidropdown>(*control);
			g_search.add_to_database(copied, this->m_child_attach_data.m_tab_name, this->m_child_attach_data.tab);

			slog::log::success("[framework::c_child] created a new control: [type: c_multidropdown, label: {}]", label);
		}

		return control;
	}

	std::shared_ptr<c_button> c_child::add_button(std::string label, std::function<void()> callback)
	{
		auto control = std::make_shared<c_button>(label, callback);
		{
			this->m_controls.push_back(control);

			// just add the control
			auto copied = std::make_shared<c_button>(*control);
			g_search.add_to_database(copied, this->m_child_attach_data.m_tab_name, this->m_child_attach_data.tab);
			slog::log::success("[framework::c_child] created a new control: [type: c_button, label: {}]", label);
		}

		return control;
	}

	std::shared_ptr<c_colorpicker> c_child::add_colorpicker(std::string label, hue::c_color* val, bool hide_label)
	{
		auto control = std::make_shared<c_colorpicker>(label, val, hide_label);
		{
			this->m_controls.push_back(control);

			// just add the control
			auto copied = std::make_shared<c_colorpicker>(*control);
			g_search.add_to_database(copied, this->m_child_attach_data.m_tab_name, this->m_child_attach_data.tab);

			//g_search.add_to_database(control, this->m_child_attach_data.m_tab_name, this->m_child_attach_data.tab);
			slog::log::success("[framework::c_child] created a new control: [type: c_colorpicker, label: {}]", label);
		}

		return control;
	}

	std::shared_ptr<c_keybind> c_child::add_keybind(std::string label, key_var_t* val, bool hide_label)
	{
		auto control = std::make_shared<c_keybind>(label, val, hide_label);
		{
			this->m_controls.push_back(control);

			// just add the control
			auto copied = std::make_shared<c_keybind>(*control);
			g_search.add_to_database(copied, this->m_child_attach_data.m_tab_name, this->m_child_attach_data.tab);
			slog::log::success("[framework::c_child] created a new control: [type: c_keybind, label: {}]", label);
		}

		return control;
	}

	std::shared_ptr<c_text_input> c_child::add_input_box(std::string label, std::string* val, bool hide_label)
	{
		auto control = std::make_shared<c_text_input>(label, val, hide_label);
		{
			this->m_controls.push_back(control);

			// just add the control
			auto copied = std::make_shared<c_text_input>(*control);
			g_search.add_to_database(copied, this->m_child_attach_data.m_tab_name, this->m_child_attach_data.tab);
			slog::log::success("[framework::c_child] created a new control: [type: c_text_input, label: {}]", label);
		}
		return control;
	}

	std::shared_ptr<c_popup> c_child::add_popup(std::string label, bool hide_label, std::function<void(c_popup* ptr)> callback)
	{
		auto control = std::make_shared<c_popup>(label, hide_label);
		{
			callback(control.get());

			this->m_controls.push_back(control);

			// just add the control
			auto copied = std::make_shared<c_popup>(*control);
			g_search.add_to_database(copied, this->m_child_attach_data.m_tab_name, this->m_child_attach_data.tab);
			slog::log::success("[framework::c_child] created a new control: [type: c_popup, label: {}]", label);
		}

		return control;
	}

	std::shared_ptr<c_interactive_preview> c_child::add_interactive_preview()
	{
		auto control = std::make_shared<c_interactive_preview>();
		{
			this->m_controls.push_back(control);
		}
		return control;
	}

	std::shared_ptr<c_listbox> c_child::add_listbox(std::string label, int* val, std::vector<std::string> items, float height, bool hide_label)
	{
		auto control = std::make_shared<c_listbox>(label, val, items, height, hide_label);
		{
			this->m_controls.push_back(control);
			slog::log::success("[framework::c_child] created a new control: [type: c_listbox, label: {}]", label);
		}

		return control;
	}
}
