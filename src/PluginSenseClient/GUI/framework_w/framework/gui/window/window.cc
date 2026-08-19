#include "../../../includes.hh"
#include <d3dcompiler.h>

namespace framework
{
	c_window::c_window(std::string title, math::c_vector_2d pos, math::c_vector_2d size) : m_title(title), m_pos(pos), m_size(size)
	{ 
		slog::log::success("[+] c_window:: {} created at pos: {}, {}, with size: {}, {}", m_title, m_pos.x, m_pos.y, m_size.x, m_size.y);
		srand((unsigned int)time(nullptr));

	}

	
	void c_window::paint()
	{
		float t = animations::m_window_opacity.val();
		float eased = t * t * (3.f - 2.f * t);

		float origin_y = this->m_pos.y;
		float origin_x = this->m_pos.x + this->m_size.x * 0.5f;

		// middle layer doesn't pass ignore_clipping so it stays on m_draw_list
		ImDrawList* dl = g_render->draw_list();
		int vtx_start = dl->VtxBuffer.Size;

		g_render->rect_shadow(this->m_pos.x, this->m_pos.y, this->m_size.x, this->m_size.y, g_style->m_window_shadow.modulate(this->m_window_opacity.limit(0.5f).val()), 15.f, 15.f);
		
		this->m_window_opacity.restore();
		
		g_render->rect_filled(this->m_pos.x, this->m_pos.y, this->m_size.x, this->m_size.y, g_style->m_window_background.modulate(this->m_window_opacity.val()), 15.f);

		g_render->push_clip(this->m_pos.x, this->m_pos.y, this->m_size.x, 45);
		g_render->rect_shadow(this->m_pos.x + 45, this->m_pos.y + 14, 20, 2, g_style->m_accent.modulate(this->m_window_opacity.limit(0.15f).val()), 70.f, 0.f);
		g_render->restore_clip();

		g_render->gradient(this->m_pos.x, this->m_pos.y + 45, this->m_size.x, 10, g_style->m_window_shadow.modulate(this->m_window_opacity.limit(0.2f).val()), g_style->m_window_shadow.modulate(this->m_window_opacity.limit(0.1f).val()).with_alpha(0), engine::fade_direction::horizontally);
		
		this->m_window_opacity.restore();

		g_font->f_icons_medium.text(this->m_pos.x + 12, this->m_pos.y + (45 * 0.5f) - (g_font->f_icons_medium.measure(ICON_FA_CLOUD_MOON).y * 0.5f), ICON_FA_CLOUD_MOON, g_style->m_accent.modulate(this->m_window_opacity.limit(1).val()));

		const float base_x = this->m_pos.x + 18 + g_font->f_icons_medium.measure(ICON_FA_CLOUD_MOON).x;
		const float title_y = this->m_pos.y + 13;

		g_font->f_default.text(base_x, title_y, "Plugin", g_style->m_text.modulate(this->m_window_opacity.limit(0.6f).val()));
		this->m_window_opacity.restore();

		g_font->f_default.text(base_x + g_font->f_default.measure("Plugin").x, title_y, "Sense", g_style->m_accent.modulate(this->m_window_opacity.limit(1.0f).val()));
		this->m_window_opacity.restore();

		g_font->f_default.text(base_x + g_font->f_default.measure("PluginSense").x, title_y, " × Green", g_style->m_text.modulate(this->m_window_opacity.limit(0.6f).val()));
		this->m_window_opacity.restore();

		g_font->f_default.text(base_x + g_font->f_default.measure("PluginSense × Green").x, title_y, "Cheat", g_style->m_accent.modulate(this->m_window_opacity.limit(1.0f).val()));
		this->m_window_opacity.restore();

		this->setup_objects();

		// this->m_preview->update(this->m_pos + math::c_vector_2d(this->m_size.x + 15, 0), math::c_vector_2d(300, this->m_size.y));
		// this->m_preview->draw();

		int vtx_end = dl->VtxBuffer.Size;

		ImDrawVert* verts = dl->VtxBuffer.Data;
		for (int v = vtx_start; v < vtx_end; v++)
		{
			verts[v].pos.x = origin_x + (verts[v].pos.x - origin_x) * eased;
			verts[v].pos.y = origin_y + (verts[v].pos.y - origin_y) * eased;

			int a = (int)(((verts[v].col >> IM_COL32_A_SHIFT) & 0xFF) * eased);
			verts[v].col = (verts[v].col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
		}
	}

	void c_window::setup_objects()
	{
		// paint tabs
		this->m_obj_tab->paint();

		// lets draw the childrens
		math::c_vector_2d layout_system = {};
		float full_childrens = 0.f;

		// children setup
		std::shared_ptr<c_child> m_parent_child = nullptr;

		for (auto& child : this->m_childrens)
		{
			// size update
			if (child->get_type() == child_width::half)
				child->m_size.x = (this->m_size.x - this->child_padding().x - 24) * 0.5f;
			else if (child->get_type() == child_width::full)
				child->m_size.x = this->m_size.x - this->child_padding().x - 10;

			// always tick opacity
			child->m_child_opacity = utils::builder::create_animation_ctx(
				child->m_name + "_child_opacity",
				child->visible() && g_ctx->m_open, 0.5f);

			float opacity = child->m_child_opacity.val();

			// fully gone ?skip entirely
			if (opacity <= 0.f)
			{
				child->m_relative_pos = {};
				child->m_pos = child->m_relative_pos + (this->m_pos + subtab_padding());
				continue;
			}

			// fading out ?draw at locked last position, skip layout contribution
			if (!child->visible())
			{
				child->draw();
				continue;
			}

			// active ?full layout + draw
			float child_bottom = layout_system.y + child->m_size.y + this->m_pos.y;
			if (child_bottom > this->m_pos.y + this->m_size.y && layout_system.y > 0)
			{
				layout_system.x += child->m_size.x + 15;
				layout_system.y = full_childrens;
			}

			child->m_relative_pos = layout_system;
			child->m_pos = child->m_relative_pos + (this->m_pos + subtab_padding());
			layout_system.y += child->m_size.y + 15;

			child->input();
			child->draw();

			//// call stack
			//if (child->fn)
			//	child->fn();

			if (child->get_type() == child_width::full)
				full_childrens += child->m_size.y + 15;
		}
	}

	void c_window::input()
	{
		this->m_window_opacity = utils::builder::create_animation_ctx(this->m_title, g_ctx->m_open, 0.5f);
		this->m_obj_tab->parent_opcity = this->m_window_opacity;

		animations::m_window_opacity = this->m_window_opacity;

		if (!g_ctx->m_open)
		{
			return;
		}

		static math::c_vector_2d prev_mouse_pos{}, delta{};

		const float screen_w = static_cast<float>(core::g_overlay->width);
		const float screen_h = static_cast<float>(core::g_overlay->height);
		this->m_pos.x = std::clamp(this->m_pos.x, 0.f, std::max(0.f, screen_w - this->m_size.x));
		this->m_pos.y = std::clamp(this->m_pos.y, 0.f, std::max(0.f, screen_h - this->m_size.y));

		math::c_rect title_bounding = math::c_rect(this->m_pos.x, this->m_pos.y, this->m_size.x - 44.f, 45.f);

		delta = prev_mouse_pos - g_input->get_mouse_position();

		if (!g_ctx->m_dragging && g_input->mouse_in_region(title_bounding.pos(), title_bounding.size()) && g_input->click_down(input::mouse_buttons::left))
		{
			g_ctx->m_dragging = true;
			prev_mouse_pos = g_input->get_mouse_position();
		}
		else if (g_ctx->m_dragging && g_input->click_down(input::mouse_buttons::left))
		{
			this->m_pos -= delta;
			this->m_pos.x = std::clamp(this->m_pos.x, 0.f, std::max(0.f, screen_w - this->m_size.x));
			this->m_pos.y = std::clamp(this->m_pos.y, 0.f, std::max(0.f, screen_h - this->m_size.y));
		}
		else if (g_ctx->m_dragging && !g_input->click_down(input::mouse_buttons::left))
		{
			g_ctx->m_dragging = false;
		}

		prev_mouse_pos = g_input->get_mouse_position();

		// update tab position when window moves
		this->m_obj_tab->update_input(this->m_pos, this->m_size);
	}

	std::shared_ptr<c_child> c_window::build_child(std::string name, child_width width, float y, std::function<void(c_child* ptr)> callback)
	{
		auto child = std::make_shared<c_child>(name, width, y);
		{
			if (!child)
			{
				slog::log::error("failed to create child: {}", name);
				return nullptr;
			}

			// attach the pos to the window's, remains to get updated in window's input but we must have atleat framed to it the moment of creation
			child->m_pos = child->m_relative_pos + this->m_pos + subtab_padding();

			// we do not call draw and input here as this only gets called once, well we just do the child caching
			this->m_childrens.push_back(child);

			callback(child.get());

			slog::log::success("[framework::c_window] created child: {}", name);
		}
		return child;
	}

	std::shared_ptr<c_tab> c_window::prebuild_tabs(std::function<void(c_tab* ptr)> callback)
	{
		auto obj = std::make_shared<c_tab>(this->m_pos, this->m_size, this->m_window_opacity);
		{
			slog::log::success("[+] prebuilded tab pointer");

			// pass the pointer
			callback(obj.get());

			// pass the pointer
			this->m_obj_tab = obj;
		}
		return obj;
	}

	void c_window::finish_tab_prebuild()
	{
		// we surely attached it now we have to update it so we wont have problems with childs, assuming the menu just initialized we dont have the problem that user interacted with it yet
		// we can safely asume that the first tab and subtab are active as menu just initialized, this way the data we pass when we attach child to tab will be valid
		g_ctx->m_cur_tab = g_ctx->m_tabs[0].m_name;

		if (g_ctx->m_tabs[0].m_subtab.empty())
		{
			g_ctx->m_tabs[0].m_cur_subtab = "";
		}
	}

	math::c_vector_2d c_window::subtab_padding()
	{
		return math::c_vector_2d(15, 60);
	}

	math::c_vector_2d c_window::child_padding()
	{
		return math::c_vector_2d(20, 65);
	}
}
