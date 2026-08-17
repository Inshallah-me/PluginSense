#include "../../../includes.hh"

#include <PluginSenseClient/Settings/MenuState.hpp>

namespace framework
{
	c_keybind::c_keybind(std::string label, key_var_t* val, bool hide_label) : m_val(val)
	{
		m_label = std::move(label);
		m_hide_label = hide_label;
		m_size = { 0, (m_hide_label ? 0.f : 16.f) };

		m_type = element_type::keybind;
		m_focus_priority = focus_priority::persistent;

		// we only use this in terms of inlining elements
		m_parent_width = 20;
	}
	
	static std::vector<std::string> modes = { "Always", "Hold", "Toggle" };
	static int imgui_key_to_vk(ImGuiKey key)
	{
		if (key >= ImGuiKey_0 && key <= ImGuiKey_9)
			return '0' + (key - ImGuiKey_0);
		if (key >= ImGuiKey_A && key <= ImGuiKey_Z)
			return 'A' + (key - ImGuiKey_A);
		if (key >= ImGuiKey_F1 && key <= ImGuiKey_F24)
			return VK_F1 + (key - ImGuiKey_F1);
		if (key >= ImGuiKey_Keypad0 && key <= ImGuiKey_Keypad9)
			return VK_NUMPAD0 + (key - ImGuiKey_Keypad0);

		switch (key)
		{
		case ImGuiKey_Tab: return VK_TAB;
		case ImGuiKey_LeftArrow: return VK_LEFT;
		case ImGuiKey_RightArrow: return VK_RIGHT;
		case ImGuiKey_UpArrow: return VK_UP;
		case ImGuiKey_DownArrow: return VK_DOWN;
		case ImGuiKey_PageUp: return VK_PRIOR;
		case ImGuiKey_PageDown: return VK_NEXT;
		case ImGuiKey_Home: return VK_HOME;
		case ImGuiKey_End: return VK_END;
		case ImGuiKey_Insert: return VK_INSERT;
		case ImGuiKey_Delete: return VK_DELETE;
		case ImGuiKey_Backspace: return VK_BACK;
		case ImGuiKey_Space: return VK_SPACE;
		case ImGuiKey_Enter: return VK_RETURN;
		case ImGuiKey_Escape: return VK_ESCAPE;
		case ImGuiKey_LeftCtrl: return VK_LCONTROL;
		case ImGuiKey_LeftShift: return VK_LSHIFT;
		case ImGuiKey_LeftAlt: return VK_LMENU;
		case ImGuiKey_RightCtrl: return VK_RCONTROL;
		case ImGuiKey_RightShift: return VK_RSHIFT;
		case ImGuiKey_RightAlt: return VK_RMENU;
		case ImGuiKey_Menu: return VK_APPS;
		case ImGuiKey_CapsLock: return VK_CAPITAL;
		case ImGuiKey_ScrollLock: return VK_SCROLL;
		case ImGuiKey_NumLock: return VK_NUMLOCK;
		case ImGuiKey_PrintScreen: return VK_SNAPSHOT;
		case ImGuiKey_Pause: return VK_PAUSE;
		case ImGuiKey_KeypadDecimal: return VK_DECIMAL;
		case ImGuiKey_KeypadDivide: return VK_DIVIDE;
		case ImGuiKey_KeypadMultiply: return VK_MULTIPLY;
		case ImGuiKey_KeypadSubtract: return VK_SUBTRACT;
		case ImGuiKey_KeypadAdd: return VK_ADD;
		default: return 0;
		}
	}

	void c_keybind::draw()
	{
		const bool can_use = enabled();
		const float enabled_alpha = can_use ? 1.f : 0.45f;

		animations::m_keybind_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5f);
		animations::m_keybind_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_keybind_value", m_visible && g_ctx->is_focused(this) && g_ctx->m_open, 0.5f);
		animations::m_keybind_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_keybind_hover", m_visible && can_use && g_ctx->m_hovered == this, 0.5f);

		// animation handling
		float target_opacity = 0.2f;
		if (animations::m_keybind_value.val() > 0.f)
			target_opacity = 0.2f + (0.6f * animations::m_keybind_value.val());
		else if (animations::m_keybind_hover.val() > 0.f)
			target_opacity = 0.2f + (0.2f * animations::m_keybind_hover.val());

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];

		float lerp_speed = 0.3f;
		smooth_opacity += (target_opacity - smooth_opacity) * lerp_speed;
		float final_opacity = animations::m_keybind_opacity.val() * smooth_opacity * enabled_alpha;

		g_render->use_layer(m_layer, [&]()
			{
				if (!m_hide_label)
				{
					// data when hide label is false its diff, if we're gonna inline elements atleast do it properly
					g_font->f_childs.text(m_pos.x, m_pos.y - 0.5f, m_label, g_style->m_text.modulate(final_opacity));
					g_font->f_icons.text(m_pos.x + (m_child_size - g_font->f_icons.measure(ICON_FA_KEYBOARD).x) - 1, m_pos.y, ICON_FA_KEYBOARD, g_style->m_text.modulate(final_opacity));
				}
				else
				{
					// imagine if we have multiinline
					g_font->f_icons.text(m_pos.x, m_pos.y, ICON_FA_KEYBOARD, g_style->m_text.modulate(final_opacity));
				}
			});

		if (animations::m_keybind_value.val() > 0.f)
		{
			const float bind_y = m_key_only ? 35.f : 70.f;
			math::c_vector_2d pallete_size = { 180, m_key_only ? 70.f : 105.f };

			// position based on the label status
			math::c_vector_2d pallete_pos = !m_hide_label
				? math::c_vector_2d(m_pos.x + m_child_size - (g_font->f_icons.measure(ICON_FA_KEYBOARD).x), m_pos.y + 20)
				: math::c_vector_2d(m_pos.x, m_pos.y + 20);

			float t = animations::m_keybind_value.val();
			float eased = t * t * (3.f - 2.f * t);

			float origin_y = pallete_pos.y;
			float origin_x = pallete_pos.x;

			ImDrawList* dl = ImGui::GetForegroundDrawList();
			int vtx_start = dl->VtxBuffer.Size;

			g_render->use_layer(engine::render_layer::prioritized, [&]() {
				// draw the background of the pallete
				g_render->rect_shadow(pallete_pos.x, pallete_pos.y, pallete_size.x, pallete_size.y, g_style->m_window_shadow.modulate(animations::m_keybind_value.val()), 10.f, 5.f);
				g_render->rect_filled(pallete_pos.x, pallete_pos.y, pallete_size.x, pallete_size.y, g_style->m_window_background.modulate(animations::m_keybind_value.val()), 5.f);

				g_render->gradient(pallete_pos.x, pallete_pos.y + 25.f, pallete_size.x, 10, g_style->m_window_shadow.modulate(animations::m_keybind_value.limit(0.2f).val()), g_style->m_window_shadow.modulate(animations::m_keybind_value.limit(0.1f).val()).with_alpha(0), engine::fade_direction::horizontally);


				//g_render->rect_filled(pallete_pos.x, pallete_pos.y, pallete_size.x, 25.f, g_style->m_window_background.modulate(animations::m_keybind_value.val()), 4.f, engine::draw_flags_::draw_flags_round_corners_top);

				// pallete name
				g_font->f_childs.text(pallete_pos.x + 8, pallete_pos.y + 3, m_label, g_style->m_text.modulate(animations::m_keybind_value.limit(0.6f).val()));
				animations::m_keybind_value.restore();

				// mode selection
				//g_render->rect_filled(pallete_pos.x + 10, pallete_pos.y + 35, pallete_size.x - 20, 25.f, g_style->m_child_top.modulate(animations::m_keybind_value.val()), 2.f);
				//g_render->rect(pallete_pos.x + 10, pallete_pos.y + 35, pallete_size.x - 20, 25.f, g_style->m_outline.modulate(animations::m_keybind_value.val()), 2.f);

				if (!m_key_only)
				{
					g_render->rect_shadow(pallete_pos.x + 10, pallete_pos.y + 35, pallete_size.x - 20, 25.f, g_style->m_window_shadow.modulate(animations::m_keybind_value.limit(0.3f).val()), 8.f, 3.f);
					animations::m_keybind_value.restore();
					g_render->rect_filled(pallete_pos.x + 10, pallete_pos.y + 35, pallete_size.x - 20, 25.f, g_style->m_element_base.modulate(animations::m_keybind_value.val()), 3.f);

					float start_x = pallete_pos.x + 15.f;
					for (int i = 0; i < modes.size(); i++)
					{
						auto mode = modes[i];

						math::rect_t bounding = math::rect_t(start_x, pallete_pos.y + 35, g_font->f_childs.measure(mode).x, 25.f);
						if (g_input->mouse_in_region(bounding.position(), bounding.size()) && g_input->clicked(input::mouse_buttons::left) && !g_ctx->m_click_consumed)
						{
							this->m_val->mode = i;
							g_ctx->m_click_consumed = true;
						}

						animations::m_keybind_selected_mode = utils::builder::create_animation_ctx(m_parent + mode + std::to_string(i) + "#m_keybind_selected_mode", this->m_val->mode == i && g_ctx->is_focused(this), 0.5f);
						g_font->f_childs.text(start_x, pallete_pos.y + 38, mode, g_style->m_text.modulate(animations::m_keybind_value.limit(0.3f).val()).lerp(g_style->m_accent.modulate(animations::m_keybind_value.limit(1.f).val()), animations::m_keybind_selected_mode.val()));

						start_x += g_font->f_childs.measure(mode).x + 10.f;
					}
				}
				
				// key selection
				// area for binding
				//g_render->rect_filled(pallete_pos.x + 10, pallete_pos.y + 70, pallete_size.x - 20, 25.f, g_style->m_child_top.modulate(animations::m_keybind_value.val()), 2.f);
				//g_render->rect(pallete_pos.x + 10, pallete_pos.y + 70, pallete_size.x - 20, 25.f, g_style->m_outline.modulate(animations::m_keybind_value.val()), 2.f);

				g_render->rect_shadow(pallete_pos.x + 10, pallete_pos.y + bind_y, pallete_size.x - 20, 25.f, g_style->m_window_shadow.modulate(animations::m_keybind_value.limit(0.3f).val()), 8.f, 3.f);
				animations::m_keybind_value.restore();
				g_render->rect_filled(pallete_pos.x + 10, pallete_pos.y + bind_y, pallete_size.x - 20, 25.f, g_style->m_element_base.modulate(animations::m_keybind_value.val()), 3.f);

				math::c_rect binding_area = math::c_rect(pallete_pos.x + 20, pallete_pos.y + bind_y, pallete_size.x - 20, 25.f);

				animations::m_keybind_binding = utils::builder::create_animation_ctx(m_parent + m_label + "#m_keybind_binding", this->m_key_callback && g_ctx->is_focused(this), 0.5f);

				g_font->f_icons.text(binding_area.x, binding_area.y + 5.f, ICON_FA_KEYBOARD, g_style->m_text.modulate(animations::m_keybind_value.limit(0.3f).val()).lerp(g_style->m_accent.modulate(animations::m_keybind_value.limit(1.f).val()), animations::m_keybind_binding.val()));

				std::string key_name = g_ctx->get_key_name(this->m_val->key);
				g_font->f_childs.text(binding_area.x + g_font->f_icons.measure(ICON_FA_KEYBOARD).x + 8.f, binding_area.y + 4.f, key_name, g_style->m_text.modulate(animations::m_keybind_value.limit(0.3f).val()).lerp(g_style->m_accent.modulate(animations::m_keybind_value.limit(1.f).val()), animations::m_keybind_binding.val()));

				//g_render->rect(pallete_pos.x - 1, pallete_pos.y - 1, pallete_size.x + 2, pallete_size.y + 2, g_style->m_outline.modulate(animations::m_keybind_value.val()), 4.f);
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
			if (!this->m_key_callback && !g_input->mouse_in_region(bounding_data.pos(), bounding_data.size()) && g_input->clicked(input::mouse_buttons::left))
			{
				g_ctx->pop_focus(this);
				this->m_key_callback = false; // we need to reset this
			}
		}
	}

	void c_keybind::input()
	{
		const auto icon_size = g_font->f_icons.measure(ICON_FA_KEYBOARD);
		math::c_rect bounding = math::c_rect(m_pos, math::c_vector_2d(m_hide_label ? icon_size.x : m_child_size, m_hide_label ? icon_size.y : m_size.y));

		if (!enabled())
		{
			if (g_ctx->m_hovered == this)
				g_ctx->m_hovered = nullptr;
			if (g_ctx->is_focused(this))
				g_ctx->pop_focus(this);
			m_key_callback = false;
			m_waiting_for_release = false;
			return;
		}

		if (!g_ctx->can_interact(this, m_focus_priority))
			return;

		if (g_input->mouse_in_region(bounding.pos(), bounding.size()))
		{
			g_ctx->m_hovered = this;
		}
		else {
			g_ctx->m_hovered = nullptr;
		}

		if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this && !g_ctx->m_click_consumed)
		{
			g_ctx->push_focus(this, m_focus_priority); // we opened it mfucker
			g_ctx->m_click_consumed = true;
			return;
		}

		if (g_ctx->is_focused(this))
		{
			const float bind_y = m_key_only ? 35.f : 70.f;
			math::c_vector_2d pallete_size = { 180, m_key_only ? 70.f : 105.f };

			// position based on the label status
			math::c_vector_2d pallete_pos = !m_hide_label
				? math::c_vector_2d(m_pos.x + m_child_size - (g_font->f_icons.measure(ICON_FA_KEYBOARD).x), m_pos.y + 20)
				: math::c_vector_2d(m_pos.x, m_pos.y + 20);

			math::c_rect binding_area = math::c_rect(pallete_pos.x + 20, pallete_pos.y + bind_y, pallete_size.x - 20, 25.f);
			math::c_rect pallete_area = math::c_rect(pallete_pos, pallete_size);

			if (!this->m_key_callback && !g_input->mouse_in_region(pallete_area.pos(), pallete_area.size()) && g_input->clicked(input::mouse_buttons::left) && !g_ctx->m_click_consumed)
			{
				g_ctx->pop_focus(this);
				this->m_key_callback = false;
				this->m_waiting_for_release = false;
				g_ctx->m_click_consumed = true;
				return;
			}

			if (!this->m_key_callback && g_input->mouse_in_region(binding_area.pos(), binding_area.size()) && g_input->clicked(input::mouse_buttons::left) && !g_ctx->m_click_consumed) {
				this->m_key_callback = true;
				this->m_waiting_for_release = true;
				for (int i = 0; i < 256; i++)
					this->m_key_down[i] = (GetAsyncKeyState(i) & 0x8000) != 0;
				g_ctx->m_click_consumed = true;
				return;
			}

			if (this->m_key_callback) {
				if (this->m_waiting_for_release) {
					bool waiting = false;
					for (int i = 0; i < 256; i++)
					{
						if (this->m_key_down[i] && (GetAsyncKeyState(i) & 0x8000) != 0)
						{
							waiting = true;
							break;
						}
					}

					if (waiting)
						return;

					for (int i = 0; i < 256; i++)
						this->m_key_down[i] = (GetAsyncKeyState(i) & 0x8000) != 0;

					this->m_waiting_for_release = false;
					return;
				}

				auto bind_key = [this](int key)
				{
					if (key == VK_ESCAPE)
						this->m_val->key = 0;
					else
						this->m_val->key = key;

					this->m_val->suppress_until_released();

					if (this->m_suppress_next_keyup)
						vars::menuKeySuppress = this->m_val->key;

					this->m_key_callback = false;
					this->m_waiting_for_release = false;
				};

				static constexpr int mouse_keys[] = {
					VK_LBUTTON,
					VK_RBUTTON,
					VK_MBUTTON,
					VK_XBUTTON1,
					VK_XBUTTON2
				};

				if (this->m_allow_mouse)
				{
					for (int key : mouse_keys) {
						bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
						bool pressed = down && !this->m_key_down[key];
						this->m_key_down[key] = down;

						if (pressed)
						{
							bind_key(key);
							break;
						}
					}
				}
				else
				{
					for (int key : mouse_keys)
					{
						this->m_key_down[key] = (GetAsyncKeyState(key) & 0x8000) != 0;
					}
				}

				for (int i = 1; this->m_key_callback && i < 255; i++) {
					bool is_mouse_key = false;
					for (int key : mouse_keys)
					{
						if (i == key)
						{
							is_mouse_key = true;
							break;
						}
					}
					if (is_mouse_key)
						continue;

					bool down = (GetAsyncKeyState(i) & 0x8000) != 0;
					bool pressed = down && !this->m_key_down[i];
					this->m_key_down[i] = down;

					if (!pressed)
						continue;

					bind_key(i);
					break;
				}
			}

		}
	}
}
