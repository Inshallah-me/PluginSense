#include "../../../includes.hh"

namespace framework
{
	c_button::c_button(std::string label, std::function<void()> callback) : m_callback(std::move(callback))
	{
		m_label = std::move(label);
		m_size = math::c_vector_2d(0, 25);

		m_type = element_type::button;
		m_focus_priority = focus_priority::interactive;

		// this element width (parent width named as it will be only accessed from parent)
		m_parent_width = m_child_size;
	}

	void c_button::draw()
	{
		animations::m_button_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5f);
		animations::m_button_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_button_value", m_visible && this->m_callback_called && g_ctx->m_open, 0.25f);
		animations::m_button_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_button_hover", m_visible && g_ctx->m_hovered == this, 0.5f);

		// animation handler
		if (animations::m_button_value.val() > 0.99f)
		{
			// we have to reset this after the animation has been finished
			// this is dogshit code i know
			this->m_callback_called = false;
		}

		// animation handling
		float target_opacity = 0.2f;
		if (animations::m_button_value.val() > 0.f)
			target_opacity = 0.2f + (0.6f * animations::m_button_value.val());
		else if (animations::m_button_hover.val() > 0.f)
			target_opacity = 0.2f + (0.2f * animations::m_button_hover.val());

		static std::unordered_map<std::string, float> smooth_opacity_cache;
		std::string opacity_key = m_parent + m_label + "#smooth_opacity";
		float& smooth_opacity = smooth_opacity_cache[opacity_key];

		float lerp_speed = 0.3f;
		smooth_opacity += (target_opacity - smooth_opacity) * lerp_speed;
		float final_opacity = animations::m_checkbox_opacity.val() * smooth_opacity;



		g_render->use_layer(m_layer, [&]()
			{
				g_render->rect_shadow(m_pos.x, m_pos.y, m_child_size, 25.f, g_style->m_window_shadow.modulate(animations::m_window_opacity.limit(0.3f).val()), 8.f, 3.f);
				animations::m_window_opacity.restore();
				g_render->rect_filled(m_pos.x, m_pos.y, m_child_size, 25.f, g_style->m_element_base.modulate(animations::m_window_opacity.val()), 3.f);

				g_font->f_childs.text(
					m_pos.x + (m_child_size * 0.5f) - (g_font->f_childs.measure(this->m_label).x * 0.5f),
					m_pos.y + 3,
					m_label,
					g_style->m_text.modulate(final_opacity).lerp(g_style->m_accent.modulate(final_opacity), animations::m_button_value.val()));

				//float dt = ImGui::GetIO().DeltaTime;
				//float speed = 2.f;
				//
				//
				//for (int i = 0; i < (int)m_ripples.size(); i++)
				//{
				//	auto& ripple = m_ripples[i];
				//	ripple.m_anim += dt * speed;
				//
				//	if (ripple.m_anim >= 1.f)
				//	{
				//		m_ripples.erase(m_ripples.begin() + i);
				//		i--;
				//		continue;
				//	}
				//
				//	float eased = 1.f - std::pow(1.f - ripple.m_anim, 3.f);
				//	float alpha = (1.f - eased) * 0.6f * animations::m_window_opacity.val();
				//	float thickness = eased * m_child_size;
				//
				//	// locked 3x3 at center
				//	float ox = m_pos.x + (m_child_size * 0.5f) - 1.5f;
				//	float oy = m_pos.y + (25.f * 0.5f) - 1.5f;
				//
				//	g_render->push_clip(m_pos.x, m_pos.y, m_child_size, 25.f);
				//	g_render->rect_shadow(
				//		ox, oy,
				//		3.f, 3.f,
				//		g_style->m_accent.modulate(alpha),
				//		thickness,
				//		2.f
				//	);
				//
				//	g_render->restore_clip();
				//}
			});

		
	}

	void c_button::input()
	{
		math::c_rect bounding = math::c_rect(m_pos, math::c_vector_2d(m_child_size, m_size.y));

		if (!g_ctx->can_interact(this, m_focus_priority))
			return;

		// we do not do return if the focus is nullptr as we want to access this
		if (g_input->mouse_in_region(bounding.pos(), bounding.size()))
		{
			g_ctx->m_hovered = this;
		}
		else if (g_ctx->m_hovered == this) {
			g_ctx->m_hovered = nullptr;
		}

		if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this && !g_ctx->m_click_consumed)
		{
			this->m_callback();
			m_callback_called = true;
			m_ripples.push_back({ 0.f });
			g_ctx->m_click_consumed = true;
		}
	}
}
