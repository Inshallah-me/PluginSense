#include "../../../includes.hh"

framework::c_interactive_preview::c_interactive_preview()
{
	m_label = "";
	m_hide_label = true;
	m_size = math::c_vector_2d(0, 19);

	m_type = element_type::interactive_preview;
	m_focus_priority = focus_priority::interactive;

	// this element width (parent width named as it will be only accessed from parent)
	m_parent_width = (30 + g_font->f_childs.measure(m_label).x);

	// push grids
	this->m_grid.push_back(std::make_shared<c_grid_system>(grid_areas::normal));
	this->m_grid.push_back(std::make_shared<c_grid_system>(grid_areas::top));
	this->m_grid.push_back(std::make_shared<c_grid_system>(grid_areas::bottom));
	this->m_grid.push_back(std::make_shared<c_grid_system>(grid_areas::left));
	this->m_grid.push_back(std::make_shared<c_grid_system>(grid_areas::right));

}

void framework::c_interactive_preview::draw()
{
	this->update_bbox();

	g_render->image(m_pos.x + 20, m_pos.y + 10, m_child_size - 40, 425, reinterpret_cast<ImTextureID>( g_texture.preview.m_srv ), hue::c_color(255, 255, 255, 255), 0.f);

	// update grid rects up front so hover detection works on real positions
	for (auto& grid : m_grid)
	{
		grid->input(this->m_bbox);
		grid->m_cursor = 0.f;
		grid->m_wcursor = 0.f;
	}

	c_base_object* dragged = g_ip_globals.m_dragging_object ? g_ip_globals.m_focused_object : nullptr;

	// keep the dragged object's pos in sync with the mouse before any hover math
	if (dragged)
		dragged->m_pos = g_input->get_mouse_position() - g_ip_globals.m_drag_offset;

	c_grid_system* hover_grid = nullptr;
	int hover_insert_idx = -1;

	auto resolve_hover = [&](c_grid_system* tg) -> bool
		{
			// resize bars to match target grid orientation before the overlap test
			if (dragged->m_type == object_types::bars)
			{
				math::c_vector_2d wanted = dragged->m_size;
				if (tg->m_area == grid_areas::top || tg->m_area == grid_areas::bottom)
					wanted = math::c_vector_2d(this->m_bbox.size().x, 4);
				else if (tg->m_area == grid_areas::left || tg->m_area == grid_areas::right)
					wanted = math::c_vector_2d(4, this->m_bbox.size().y);

				math::c_vector_2d test_pos = g_input->get_mouse_position() - wanted * 0.5f;
				if (!tg->object_in_area(test_pos, wanted))
					return false;

				if (wanted.x != dragged->m_size.x || wanted.y != dragged->m_size.y)
				{
					dragged->m_size = wanted;
					g_ip_globals.m_drag_offset = wanted * 0.5f;
				}
				dragged->m_pos = g_input->get_mouse_position() - g_ip_globals.m_drag_offset;
			}
			else
			{
				if (!tg->object_in_area(dragged->m_pos, dragged->m_size))
					return false;
			}

			hover_grid = tg;
			dragged->m_drop_state = tg;

			bool horizontal = (tg->m_area == grid_areas::left || tg->m_area == grid_areas::right);
			math::c_vector_2d drag_center = dragged->m_pos + dragged->m_size * 0.5f;

			float cursor = 0.f;
			int idx = 0;

			for (auto& other : tg->m_objects)
			{
				if (other.get() == dragged) continue;
				if (other->m_type == object_types::box) continue;
				if (other->m_visible_flag && !*other->m_visible_flag) continue;

				// stride along the stacking axis
				float stride = horizontal
					? ((other->m_type == object_types::bars) ? 6.f : (other->m_size.y + 2.f))
					: (other->m_size.y + 2.f);

				// midpoint of this slot in the layout's local cursor space
				float slot_mid = cursor + stride * 0.5f;

				float local;
				if (tg->m_area == grid_areas::top)
					local = (this->m_bbox.pos().y - drag_center.y) - (dragged->m_size.y * 0.5f);
				else if (tg->m_area == grid_areas::bottom)
					local = drag_center.y - (this->m_bbox.pos().y + this->m_bbox.size().y + 2.f);
				else // left or right -- vertical stacking inside a side strip
					local = drag_center.y - this->m_bbox.pos().y;

				if (local < slot_mid) break;

				cursor += stride;
				idx++;
			}

			hover_insert_idx = idx;
			g_ip_globals.m_insert_index = idx;
			return true;
		};

	if (dragged)
	{
		// non-normal grids first
		for (auto& tg : m_grid)
		{
			if (tg->m_area == grid_areas::normal) continue;
			if (resolve_hover(tg.get())) break;
		}
		// fall back to normal
		if (!hover_grid)
		{
			for (auto& tg : m_grid)
			{
				if (tg->m_area != grid_areas::normal) continue;
				if (resolve_hover(tg.get())) break;
			}
		}

		if (!hover_grid)
			dragged->m_drop_state = nullptr;
	}

	c_base_object* pending_move_obj = nullptr;
	c_grid_system* pending_move_src = nullptr;
	c_grid_system* pending_move_dst = nullptr;

	for (auto grid : m_grid)
	{
		int slot = 0;

		for (auto obj : grid->m_objects)
		{
			if (obj->m_visible_flag && !*obj->m_visible_flag)
				continue;

			obj->m_bbox = this->m_bbox;

			bool is_focused = g_ip_globals.m_focused_object == obj.get();
			bool can_interact = !g_ip_globals.m_dragging_object || is_focused;

			if (obj->m_type == object_types::text)
			{
				static int old_font = obj->m_custom_font;
				if (old_font != obj->m_custom_font)
				{
					obj->m_size = { obj->m_font.measure(obj->m_label).x, obj->m_font.measure(obj->m_label).y };
					old_font = obj->m_custom_font;
				}
			}

			// box always fills bbox
			if (obj->m_type == object_types::box)
			{
				obj->m_pos = this->m_bbox.pos();
				obj->m_size = this->m_bbox.size();
			}
			else if (!is_focused)
			{
				// open a gap before this object if the dragged item will land here
				if (grid.get() == hover_grid && slot == hover_insert_idx && dragged)
				{
					bool side = (grid->m_area == grid_areas::left || grid->m_area == grid_areas::right);
					if (side && dragged->m_type == object_types::bars)
						grid->m_wcursor += dragged->m_size.x + 2.f;
					else
						grid->m_cursor += dragged->m_size.y + 2.f;
				}

				if (obj->m_type == object_types::text)
				{
					if (grid->m_area == grid_areas::top)
					{
						grid->m_relative_pos = this->m_bbox.pos() + math::c_vector_2d(
							this->m_bbox.size().x * 0.5f - obj->m_size.x * 0.5f,
							-(obj->m_size.y + 2.f) - grid->m_cursor
						);
					}
					else if (grid->m_area == grid_areas::bottom)
					{
						grid->m_relative_pos = this->m_bbox.pos() + math::c_vector_2d(
							this->m_bbox.size().x * 0.5f - obj->m_size.x * 0.5f,
							this->m_bbox.size().y + grid->m_cursor
						);
					}
					else if (grid->m_area == grid_areas::left)
					{
						grid->m_relative_pos = this->m_bbox.pos() - math::c_vector_2d(
							obj->m_size.x + 2.f + grid->m_wcursor, -grid->m_cursor
						);
					}
					else if (grid->m_area == grid_areas::right)
					{
						grid->m_relative_pos = this->m_bbox.pos() + math::c_vector_2d(
							this->m_bbox.size().x + 2.f + grid->m_wcursor, grid->m_cursor
						);
					}

					grid->m_cursor += obj->m_size.y + 2.f;
				}
				else if (obj->m_type == object_types::bars)
				{
					if (grid->m_area == grid_areas::top)
					{
						obj->m_size = math::c_vector_2d(this->m_bbox.size().x, 4);
						grid->m_relative_pos = this->m_bbox.pos() - math::c_vector_2d(0.f, obj->m_size.y + 2.f + grid->m_cursor);
						grid->m_cursor += obj->m_size.y + 2.f;
					}
					else if (grid->m_area == grid_areas::bottom)
					{
						obj->m_size = math::c_vector_2d(this->m_bbox.size().x, 4);
						grid->m_relative_pos = this->m_bbox.pos() + math::c_vector_2d(0.f, this->m_bbox.size().y + 2.f + grid->m_cursor);
						grid->m_cursor += obj->m_size.y + 2.f;
					}
					else if (grid->m_area == grid_areas::left)
					{
						obj->m_size = math::c_vector_2d(4, this->m_bbox.size().y);
						grid->m_relative_pos = this->m_bbox.pos() - math::c_vector_2d(obj->m_size.x + 2.f + grid->m_wcursor, 0.f);
						grid->m_wcursor += 6.f;
					}
					else if (grid->m_area == grid_areas::right)
					{
						obj->m_size = math::c_vector_2d(4, this->m_bbox.size().y);
						grid->m_relative_pos = this->m_bbox.pos() + math::c_vector_2d(this->m_bbox.size().x + 2.f + grid->m_wcursor, 0.f);
						grid->m_wcursor += 6.f;
					}
				}

				obj->m_pos = grid->m_relative_pos;
				slot++;
			}

			// hover/click handling
			if (can_interact)
			{
				if (g_input->mouse_in_region(obj->m_pos, obj->m_size))
				{
					if (g_input->click_down(input::mouse_buttons::left) && !g_ip_globals.m_dragging_object)
					{
						g_ip_globals.m_focused_object = obj.get();
						g_ip_globals.m_dragging_object = true;
						g_ip_globals.m_drag_offset = g_input->get_mouse_position() - obj->m_pos;
					}

					// right-click opens the attached popup, if any
					if (obj->m_popup && g_input->clicked(input::mouse_buttons::right) && !g_ip_globals.m_dragging_object)
					{
						obj->m_popup->m_pos = obj->m_pos + math::c_vector_2d(obj->m_size.x + 4.f, 0.f);
						obj->m_popup->set_parent(this->m_label + "#ip_ctx#");
						obj->m_popup->set_visibility(true);
						g_ctx->push_focus(obj->m_popup.get(), focus_priority::modal);
						g_ctx->m_modal_owner = obj->m_popup.get();
					}

					if (!g_ip_globals.m_dragging_object)
						g_ip_globals.m_hovered_object = obj.get();
				}
				else if (!is_focused)
				{
					if (g_ip_globals.m_hovered_object == obj.get())
						g_ip_globals.m_hovered_object = nullptr;
				}
			}

			// release: queue the move using the pre-computed insert index
			if (g_input->click_released(input::mouse_buttons::left) && is_focused)
			{
				obj->m_target_pos = {};

				if (obj->m_drop_state)
				{
					pending_move_obj = obj.get();
					pending_move_src = grid.get();
					pending_move_dst = obj->m_drop_state;
				}

				obj->m_drop_state = nullptr;
				g_ip_globals.m_focused_object = nullptr;
				g_ip_globals.m_dragging_object = false;
			}

			// ghost target rect at the would-be insert position
			if (g_ip_globals.m_dragging_object && is_focused && hover_grid)
			{
				// virtually walk hover_grid up to the insert slot using the same
				// stride logic as pass 1 / layout
				bool side = (hover_grid->m_area == grid_areas::left || hover_grid->m_area == grid_areas::right);
				float ghost_cursor = 0.f;
				float ghost_wcursor = 0.f;
				int s = 0;

				for (auto& other : hover_grid->m_objects)
				{
					if (other.get() == obj.get()) continue;
					if (other->m_type == object_types::box) continue;
					if (other->m_visible_flag && !*other->m_visible_flag) continue;
					if (s == hover_insert_idx) break;

					if (side && other->m_type == object_types::bars)
						ghost_wcursor += 6.f;
					else
						ghost_cursor += other->m_size.y + 2.f;
					s++;
				}

				if (hover_grid->m_area == grid_areas::top)
				{
					obj->m_target_pos = this->m_bbox.pos() + math::c_vector_2d(
						this->m_bbox.size().x * 0.5f - obj->m_size.x * 0.5f,
						-(obj->m_size.y + 2.f) - ghost_cursor
					);
				}
				else if (hover_grid->m_area == grid_areas::bottom)
				{
					obj->m_target_pos = this->m_bbox.pos() + math::c_vector_2d(
						this->m_bbox.size().x * 0.5f - obj->m_size.x * 0.5f,
						this->m_bbox.size().y + 2.f + ghost_cursor
					);
				}
				else if (hover_grid->m_area == grid_areas::left)
				{
					obj->m_target_pos = this->m_bbox.pos() - math::c_vector_2d(
						obj->m_size.x + 2.f + ghost_wcursor, -ghost_cursor
					);
				}
				else if (hover_grid->m_area == grid_areas::right)
				{
					obj->m_target_pos = this->m_bbox.pos() + math::c_vector_2d(
						this->m_bbox.size().x + 2.f + ghost_wcursor, ghost_cursor
					);
				}
			}

			obj->input();
			obj->draw();
		}
	}

	// apply the queued move
	if (pending_move_obj)
	{
		auto it = std::find_if(pending_move_src->m_objects.begin(), pending_move_src->m_objects.end(),
			[&](const std::shared_ptr<c_base_object>& o) { return o.get() == pending_move_obj; });

		if (it != pending_move_src->m_objects.end())
		{
			auto obj_ptr = *it;

			pending_move_src->m_objects.erase(it);

			int idx = std::clamp(g_ip_globals.m_insert_index, 0, (int)pending_move_dst->m_objects.size());

			// for left/right grids, force bars before text
			if (obj_ptr->m_type == object_types::bars &&
				(pending_move_dst->m_area == grid_areas::left || pending_move_dst->m_area == grid_areas::right))
			{
				int bar_insert = 0;
				for (int i = 0; i < (int)pending_move_dst->m_objects.size(); i++)
				{
					if (pending_move_dst->m_objects[i]->m_type == object_types::bars)
						bar_insert = i + 1;
					else
						break;
				}
				idx = bar_insert;
			}

			pending_move_dst->m_objects.insert(
				pending_move_dst->m_objects.begin() + idx,
				obj_ptr
			);
		}
	}

	for (auto& grid : m_grid)
	{
		for (auto& obj : grid->m_objects)
		{
			if (!obj->m_popup)
				continue;

			obj->m_popup->m_child_size = 20.f; // popup uses this as the icon-side width
			if (g_ctx->m_open)
				obj->m_popup->input();
			obj->m_popup->draw();
		}
	}
}

void framework::c_interactive_preview::input()
{

}

void framework::c_interactive_preview::update_bbox()
{
	math::c_vector_2d position = math::c_vector_2d(m_pos.x + 20, m_pos.y + 10);
	math::c_vector_2d maxs = math::c_vector_2d(m_child_size - 40, 425);

	this->m_bbox = math::c_rect(position.x + 32.f, position.y + 25.f, maxs.x - 64.f, maxs.y - 35.f);
}

std::shared_ptr<framework::c_bounding_box> framework::c_interactive_preview::add_box()
{
	auto control = std::make_shared<c_bounding_box>();
	{
		// push to normal grid
		this->m_grid[0]->m_objects.push_back(control);
	}

	return control;
}

std::shared_ptr<framework::c_text_object> framework::c_interactive_preview::add_text_object(std::string label, grid_areas area)
{
	auto control = std::make_shared<c_text_object>(label);
	{
		// push to normal grid
		this->m_grid[(int)area]->m_objects.push_back(control);
	}

	return control;
}

std::shared_ptr<framework::c_bar_object> framework::c_interactive_preview::add_bar_object(std::string label, hue::c_color default_color, grid_areas area)
{
	auto control = std::make_shared<c_bar_object>(label, default_color);
	{
		// push to normal grid
		this->m_grid[(int)area]->m_objects.push_back(control);
	}

	return control;
}

framework::c_grid_system::c_grid_system(grid_areas area)
{
	this->m_area = area;
}

void framework::c_grid_system::draw()
{
	if (m_pos.x == 0 && m_pos.y == 0)
		return;

	g_render->rect_filled(m_pos.x, m_pos.y, m_size.x, m_size.y, hue::c_color(255, 255, 255, 10));
}

void framework::c_grid_system::input(math::c_rect m_bbox)
{
	if (this->m_area == grid_areas::normal) {
		this->m_pos = m_bbox.pos();
		this->m_size = m_bbox.size();
	}
	else if (this->m_area == grid_areas::top) {
		this->m_pos.x = m_bbox.pos().x;
		this->m_pos.y = m_bbox.pos().y - 30.f;

		this->m_size.x = m_bbox.size().x;
		this->m_size.y = 30.f;
	}
	else if (this->m_area == grid_areas::bottom) {
		this->m_pos.x = m_bbox.pos().x;
		this->m_pos.y = m_bbox.pos().y + m_bbox.size().y + 2.f;

		this->m_size.x = m_bbox.size().x;
		this->m_size.y = 30.f;
	}
	else if (this->m_area == grid_areas::left) {
		this->m_pos.x = m_bbox.pos().x - 30.f;
		this->m_pos.y = m_bbox.pos().y;

		this->m_size.x = 30.f;
		this->m_size.y = m_bbox.size().y;
	}
	else if (this->m_area == grid_areas::right) {
		this->m_pos.x = m_bbox.pos().x + m_bbox.size().x + 2.f;
		this->m_pos.y = m_bbox.pos().y;

		this->m_size.x = 30.f;
		this->m_size.y = m_bbox.size().y;
	}
}

void framework::c_bounding_box::draw()
{
	g_render->rect(m_bbox.x, m_bbox.y, m_bbox.w, m_bbox.h, hue::c_color());
	g_render->rect(m_bbox.x - 1, m_bbox.y - 1, m_bbox.w + 2, m_bbox.h + 2, hue::c_color(0, 0, 0));
	g_render->rect(m_bbox.x + 1, m_bbox.y + 1, m_bbox.w - 2, m_bbox.h - 2, hue::c_color(0, 0, 0));

	g_render->rect_shadow(m_bbox.x - 1, m_bbox.y - 1, m_bbox.w + 2, m_bbox.h + 2, hue::c_color(255, 255, 255, (int)(this->m_hover_animation.val() * 50.f)), 10.f, 0.f);
}

void framework::c_bounding_box::input()
{
	m_hover_animation = utils::builder::create_animation_ctx("box_hover", g_ip_globals.m_hovered_object == this, 0.5f);
}

void framework::c_text_object::draw()
{
	m_font.text(m_pos.x, m_pos.y, m_label, hue::c_color(255, 255, 255), this->m_custom_font == 0 ? engine::modifiers::drop_shadow : engine::modifiers::outline);

	g_render->rect_shadow(m_pos.x - 1, m_pos.y - 1, m_size.x + 2, m_size.y + 2, hue::c_color(255, 255, 255, (int)(this->m_hover_animation.val() * 50.f)), 10.f, 0.f);

	// ghost at snap target
	if (m_target_pos.x != 0.f || m_target_pos.y != 0.f)
	{
		g_render->rect_shadow(
			m_target_pos.x - 1, m_target_pos.y - 1,
			m_size.x + 2, m_size.y + 2,
			hue::c_color(255, 255, 255, 50),
			10.f, 0.f
		);
	}
}

void framework::c_text_object::input()
{
	m_hover_animation = utils::builder::create_animation_ctx(m_label + "element", g_ip_globals.m_hovered_object == this, 0.5f);

	// m_size = { m_font.measure(m_label).x, m_font.measure(m_label).y };
}

void framework::c_bar_object::draw()
{
	g_render->rect_filled(m_pos.x, m_pos.y, m_size.x, m_size.y, hue::c_color(0, 0, 0, 120));
	g_render->rect_filled(m_pos.x + 1, m_pos.y + 1, m_size.x - 2, m_size.y - 2, m_def_color);

	g_render->rect_shadow(
		m_target_pos.x - 1, m_target_pos.y - 1,
		m_size.x + 2, m_size.y + 2,
		hue::c_color(255, 255, 255, 50),
		10.f, 0.f
	);
}

void framework::c_bar_object::input()
{
	m_hover_animation = utils::builder::create_animation_ctx(m_label, g_ip_globals.m_hovered_object == this, 0.5f);
}

std::vector<framework::laid_out_element_t> framework::c_interactive_preview::extract_layout(
	math::c_rect& target_box,
	std::function<math::c_vector_2d(c_base_object*)> size_for_text) const
{
	std::vector<laid_out_element_t> out;

	for (auto& grid : m_grid)
	{
		float cursor = 0.f;
		float wcursor = 0.f;

		for (auto& obj : grid->m_objects)
		{
			// visibility + box skip (box is just the bbox marker, not a real element)
			if (obj->m_visible_flag && !*obj->m_visible_flag) continue;
			if (obj->m_type == object_types::box) continue;

			// resolve the size we'll use for this object
			math::c_vector_2d item_size = obj->m_size;
			if (obj->m_type == object_types::text && size_for_text)
			{
				math::c_vector_2d override_size = size_for_text(obj.get());
				if (override_size.x > 0.f && override_size.y > 0.f)
					item_size = override_size;
			}
			else if (obj->m_type == object_types::bars)
			{
				// bars size themselves relative to the target box, same rule the
				// main draw pass uses
				if (grid->m_area == grid_areas::top || grid->m_area == grid_areas::bottom)
					item_size = math::c_vector_2d(target_box.size().x, 4.f);
				else if (grid->m_area == grid_areas::left || grid->m_area == grid_areas::right)
					item_size = math::c_vector_2d(4.f, target_box.size().y);
			}

			laid_out_element_t el{};
			el.m_object = obj.get();
			el.m_label = obj->m_label;
			el.m_type = obj->m_type;
			el.m_size = item_size;

			if (obj->m_type == object_types::text)
			{
				if (grid->m_area == grid_areas::top)
				{
					el.m_pos = target_box.pos() + math::c_vector_2d(
						target_box.size().x * 0.5f - item_size.x * 0.5f,
						-(item_size.y + 2.f) - cursor
					);
				}
				else if (grid->m_area == grid_areas::bottom)
				{
					el.m_pos = target_box.pos() + math::c_vector_2d(
						target_box.size().x * 0.5f - item_size.x * 0.5f,
						target_box.size().y + cursor
					);
				}
				else if (grid->m_area == grid_areas::left)
				{
					el.m_pos = target_box.pos() - math::c_vector_2d(
						item_size.x + 2.f + wcursor, -cursor
					);
				}
				else if (grid->m_area == grid_areas::right)
				{
					el.m_pos = target_box.pos() + math::c_vector_2d(
						target_box.size().x + 2.f + wcursor, cursor
					);
				}

				cursor += item_size.y + 2.f;
			}
			else if (obj->m_type == object_types::bars)
			{
				if (grid->m_area == grid_areas::top)
				{
					el.m_pos = target_box.pos() - math::c_vector_2d(0.f, item_size.y + 2.f + cursor);
					cursor += item_size.y + 2.f;
				}
				else if (grid->m_area == grid_areas::bottom)
				{
					el.m_pos = target_box.pos() + math::c_vector_2d(0.f, target_box.size().y + 2.f + cursor);
					cursor += item_size.y + 2.f;
				}
				else if (grid->m_area == grid_areas::left)
				{
					el.m_pos = target_box.pos() - math::c_vector_2d(item_size.x + 2.f + wcursor, 0.f);
					wcursor += item_size.x + 2.f;
				}
				else if (grid->m_area == grid_areas::right)
				{
					el.m_pos = target_box.pos() + math::c_vector_2d(target_box.size().x + 2.f + wcursor, 0.f);
					wcursor += item_size.x + 2.f;
				}
			}

			out.push_back(el);
		}
	}

	return out;
}

framework::laid_out_element_t framework::c_interactive_preview::resolve(
	const std::string& label,
	math::c_rect& target_box,
	std::function<math::c_vector_2d(c_base_object*)> size_for_text) const
{
	auto elements = extract_layout(target_box, size_for_text);
	for (auto& el : elements)
	{
		if (el.m_label == label)
			return el;
	}
	return laid_out_element_t{};
}

