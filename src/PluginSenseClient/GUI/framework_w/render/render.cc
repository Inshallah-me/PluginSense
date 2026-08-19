#include "../includes.hh"

namespace
{
	const ImWchar* framework_text_ranges()
	{
		static const ImWchar ranges[] =
		{
			0x0020, 0x00FF,
			0x0100, 0x024F,
			0x0400, 0x052F,
			0x2000, 0x206F,
			0x3000, 0x30FF,
			0x3130, 0x318F,
			0x31F0, 0x31FF,
			0x4E00, 0x9FFF,
			0xFF00, 0xFFEF,
			0,
		};

		return ranges;
	}

	std::string windows_font_path(const char* file_name)
	{
		char windows_dir[MAX_PATH]{};
		if (GetWindowsDirectoryA(windows_dir, MAX_PATH) == 0)
			return {};

		std::string path = windows_dir;
		path += "\\Fonts\\";
		path += file_name;
		return path;
	}

	void merge_system_font(float size, unsigned int builder_flags)
	{
		ImGuiIO& io = ImGui::GetIO();

		ImFontConfig merge_cfg{};
		merge_cfg.MergeMode = true;
		merge_cfg.FontBuilderFlags = builder_flags;

		const std::string segoe = windows_font_path("segoeui.ttf");
		if (!segoe.empty() && GetFileAttributesA(segoe.c_str()) != INVALID_FILE_ATTRIBUTES)
			io.Fonts->AddFontFromFileTTF(segoe.c_str(), size, &merge_cfg, framework_text_ranges());

		const std::string yahei = windows_font_path("msyh.ttc");
		if (!yahei.empty() && GetFileAttributesA(yahei.c_str()) != INVALID_FILE_ATTRIBUTES)
			io.Fonts->AddFontFromFileTTF(yahei.c_str(), size, &merge_cfg, framework_text_ranges());
	}
}

math::c_vector_2d input::c_input_controller::get_mouse_position()
{
	return math::c_vector_2d(ImGui::GetMousePos().x, ImGui::GetMousePos().y);
}

bool input::c_input_controller::mouse_in_region(const math::c_vector_2d& pos, const math::c_vector_2d& size)
{
	return (ImGui::GetIO().MousePos.x > pos.x && ImGui::GetIO().MousePos.y > pos.y &&
		ImGui::GetIO().MousePos.x < pos.x + size.x && ImGui::GetIO().MousePos.y < pos.y + size.y);
}

bool input::c_input_controller::clicked(mouse_buttons button)
{
	return ImGui::IsMouseClicked(ImGuiMouseButton(button));
}

bool input::c_input_controller::click_down(mouse_buttons button)
{
	return ImGui::IsMouseDown(ImGuiMouseButton(button));
}

bool input::c_input_controller::click_released(mouse_buttons button)
{
	return ImGui::IsMouseReleased(ImGuiMouseButton(button));
}

float input::c_input_controller::get_wheel_value()
{
	return ImGui::GetIO().MouseWheel;
}

/* layering system */
void c_render::begin_layers()
{
	//if (this->m_draw_list == ImGui::GetForegroundDrawList())
	//	return;

	this->m_draw_list->ChannelsSplit((int)engine::render_layer::count);
}

void c_render::set_layer(engine::render_layer layer)
{
	//if (this->m_draw_list == ImGui::GetForegroundDrawList())
	//	return;

	this->m_draw_list->ChannelsSetCurrent((int)layer);
}

void c_render::end_layers()
{
	//if (this->m_draw_list == ImGui::GetForegroundDrawList())
	//	return;

	this->m_draw_list->ChannelsMerge();
}

void c_render::rect_shadow(float x, float y, float w, float h, hue::c_color col, float thickness, float rouding)
{
	auto offset = ImVec2{ 1.0f, 1.0f };
	auto rounding = rouding;
	auto spread = thickness;

	ImU32 c_full = IM_COL32(col.r, col.g, col.b, col.a);
	ImU32 c_near = IM_COL32(col.r, col.g, col.b, (int)(col.a * 0.35f));
	ImU32 c_far = IM_COL32(col.r, col.g, col.b, (int)(col.a * 0.08f));
	ImU32 c_zero = IM_COL32(col.r, col.g, col.b, 0);
	ImVec2 uv = m_draw_list->_Data->TexUvWhitePixel;

	w -= 2;
	h -= 2;

	float sx = x + offset.x;
	float sy = y + offset.y;
	float ex = x + w + offset.x;
	float ey = y + h + offset.y;

	float r = std::min(rounding, std::min(w * 0.5f, h * 0.5f));

	float s1 = spread * 0.33f;
	float s2 = spread * 0.66f;
	float s3 = spread;

	float r1 = r + s1, r2 = r + s2, r3 = r + s3;

	auto quad = [&](ImVec2 a, ImVec2 b, ImVec2 c, ImVec2 d,
		ImU32 ca, ImU32 cb, ImU32 cc, ImU32 cd)
		{
			m_draw_list->PrimReserve(6, 4);
			ImDrawIdx idx = (ImDrawIdx)m_draw_list->_VtxCurrentIdx;
			m_draw_list->PrimWriteVtx(a, uv, ca);
			m_draw_list->PrimWriteVtx(b, uv, cb);
			m_draw_list->PrimWriteVtx(c, uv, cc);
			m_draw_list->PrimWriteVtx(d, uv, cd);
			m_draw_list->PrimWriteIdx(idx);
			m_draw_list->PrimWriteIdx(idx + 1);
			m_draw_list->PrimWriteIdx(idx + 2);
			m_draw_list->PrimWriteIdx(idx);
			m_draw_list->PrimWriteIdx(idx + 2);
			m_draw_list->PrimWriteIdx(idx + 3);
		};

	float clx = sx + r, crx = ex - r;
	float cty = sy + r, cby = ey - r;

	auto edge_quads = [&](ImVec2 i_a, ImVec2 i_b,
		ImVec2 n_a, ImVec2 n_b,
		ImVec2 f_a, ImVec2 f_b,
		ImVec2 o_a, ImVec2 o_b)
		{
			quad(i_a, i_b, n_b, n_a, c_full, c_full, c_near, c_near);
			quad(n_a, n_b, f_b, f_a, c_near, c_near, c_far, c_far);
			quad(f_a, f_b, o_b, o_a, c_far, c_far, c_zero, c_zero);
		};

	edge_quads(
		{ clx, sy }, { crx, sy },
		{ clx, sy - s1 }, { crx, sy - s1 },
		{ clx, sy - s2 }, { crx, sy - s2 },
		{ clx, sy - s3 }, { crx, sy - s3 }
	);

	edge_quads(
		{ crx, ey }, { clx, ey },
		{ crx, ey + s1 }, { clx, ey + s1 },
		{ crx, ey + s2 }, { clx, ey + s2 },
		{ crx, ey + s3 }, { clx, ey + s3 }
	);

	edge_quads(
		{ sx, cby }, { sx, cty },
		{ sx - s1, cby }, { sx - s1, cty },
		{ sx - s2, cby }, { sx - s2, cty },
		{ sx - s3, cby }, { sx - s3, cty }
	);

	edge_quads(
		{ ex, cty }, { ex, cby },
		{ ex + s1, cty }, { ex + s1, cby },
		{ ex + s2, cty }, { ex + s2, cby },
		{ ex + s3, cty }, { ex + s3, cby }
	);


	const int seg = 16;
	auto corner = [&](ImVec2 center, float a_start)
		{
			m_draw_list->PrimReserve(seg * 18, seg * 4 + 4);
			ImDrawIdx base = (ImDrawIdx)m_draw_list->_VtxCurrentIdx;

			for (int i = 0; i <= seg; i++) {
				float a = a_start + (IM_PI * 0.5f) * (float)i / seg;
				float cs = std::cos(a);
				float sn = std::sin(a);
				m_draw_list->PrimWriteVtx({ center.x + cs * r,  center.y + sn * r }, uv, c_full);
				m_draw_list->PrimWriteVtx({ center.x + cs * r1, center.y + sn * r1 }, uv, c_near);
				m_draw_list->PrimWriteVtx({ center.x + cs * r2, center.y + sn * r2 }, uv, c_far);
				m_draw_list->PrimWriteVtx({ center.x + cs * r3, center.y + sn * r3 }, uv, c_zero);
			}

			for (int i = 0; i < seg; i++) {
				ImDrawIdx v = base + i * 4;

				m_draw_list->PrimWriteIdx(v + 0); m_draw_list->PrimWriteIdx(v + 1); m_draw_list->PrimWriteIdx(v + 4);
				m_draw_list->PrimWriteIdx(v + 4); m_draw_list->PrimWriteIdx(v + 1); m_draw_list->PrimWriteIdx(v + 5);

				m_draw_list->PrimWriteIdx(v + 1); m_draw_list->PrimWriteIdx(v + 2); m_draw_list->PrimWriteIdx(v + 5);
				m_draw_list->PrimWriteIdx(v + 5); m_draw_list->PrimWriteIdx(v + 2); m_draw_list->PrimWriteIdx(v + 6);

				m_draw_list->PrimWriteIdx(v + 2); m_draw_list->PrimWriteIdx(v + 3); m_draw_list->PrimWriteIdx(v + 6);
				m_draw_list->PrimWriteIdx(v + 6); m_draw_list->PrimWriteIdx(v + 3); m_draw_list->PrimWriteIdx(v + 7);
			}
		};

	corner({ crx, cty }, -IM_PI * 0.5f);
	corner({ clx, cty }, -IM_PI);
	corner({ clx, cby }, IM_PI * 0.5f);
	corner({ crx, cby }, 0.f);
}


void c_render::rect_filled(float x, float y, float w, float h, hue::c_color col, float rounding, engine::draw_flags flags)
{
	this->m_draw_list->AddRectFilled(ImVec2(static_cast<float>(x), static_cast<float>(y)), ImVec2(static_cast<float>(x + w), static_cast<float>(y + h)),
		col.transform(),
		rounding, flags
	);
}

void c_render::rect(float x, float y, float w, float h, hue::c_color col, float rounding, float thickness)
{
	this->m_draw_list->AddRect(ImVec2(static_cast<float>(x), static_cast<float>(y)), ImVec2(static_cast<float>(x + w), static_cast<float>(y + h)),
		col.transform(), rounding, 0, thickness
	);
}

void c_render::image(float x, float y, float w, float h, ImTextureID texture_id, hue::c_color col, float rounding)
{
	if (rounding > 0.f)
	{
		this->m_draw_list->AddImageRounded(texture_id, ImVec2(static_cast<float>(x), static_cast<float>(y)), ImVec2(static_cast<float>(x + w), static_cast<float>(y + h)), ImVec2(0.f, 0.f), ImVec2(1.f, 1.f),
			col.transform(),
			rounding
		);
	}
	else
	{
		this->m_draw_list->AddImage(texture_id, ImVec2(static_cast<float>(x), static_cast<float>(y)), ImVec2(static_cast<float>(x + w), static_cast<float>(y + h)), ImVec2(0.f, 0.f), ImVec2(1.f, 1.f),
			col.transform()
		);
	}
}

void c_render::set_linear_alpha(int vert_start_idx, int vert_end_idx, ImVec2 gradient_p0, ImVec2 gradient_p1, ImU32 col0, ImU32 col1)
{
	ImVec2 gradient_extent = gradient_p1 - gradient_p0;
	float gradient_inv_length2 = 1.0f / ImLengthSqr(gradient_extent);
	ImDrawVert* vert_start = this->m_draw_list->VtxBuffer.Data + vert_start_idx;
	ImDrawVert* vert_end = this->m_draw_list->VtxBuffer.Data + vert_end_idx;
	const int col0_r = (int)(col0 >> IM_COL32_R_SHIFT) & 0xFF;
	const int col0_g = (int)(col0 >> IM_COL32_G_SHIFT) & 0xFF;
	const int col0_b = (int)(col0 >> IM_COL32_B_SHIFT) & 0xFF;
	const int col0_a = (int)(col0 >> IM_COL32_A_SHIFT) & 0xFF;
	const int col_delta_r = ((int)(col1 >> IM_COL32_R_SHIFT) & 0xFF) - col0_r;
	const int col_delta_g = ((int)(col1 >> IM_COL32_G_SHIFT) & 0xFF) - col0_g;
	const int col_delta_b = ((int)(col1 >> IM_COL32_B_SHIFT) & 0xFF) - col0_b;
	const int col_delta_a = ((int)(col1 >> IM_COL32_A_SHIFT) & 0xFF) - col0_a;
	for (ImDrawVert* vert = vert_start; vert < vert_end; vert++)
	{
		float d = ImDot(vert->pos - gradient_p0, gradient_extent);
		float t = ImClamp(d * gradient_inv_length2, 0.0f, 1.0f);
		int r = (int)(col0_r + col_delta_r * t);
		int g = (int)(col0_g + col_delta_g * t);
		int b = (int)(col0_b + col_delta_b * t);
		int a = (int)(col0_a + col_delta_a * t);
		vert->col = (r << IM_COL32_R_SHIFT) | (g << IM_COL32_G_SHIFT) | (b << IM_COL32_B_SHIFT) | (a << IM_COL32_A_SHIFT);
	}
}


void c_render::fade_rect(float x, float y, float w, float h, hue::c_color col, hue::c_color col2, engine::fade_direction direction, float rounding, float thickness)
{
	int vtx_buffer_start = this->m_draw_list->VtxBuffer.Size;
	this->m_draw_list->AddRect(ImVec2(static_cast<float>(x), static_cast<float>(y)), ImVec2(static_cast<float>(x + w), static_cast<float>(y + h)),
		col.transform(), rounding, 0, thickness
	);
	int vtx_buffer_end = this->m_draw_list->VtxBuffer.Size;

	const ImVec2 fade_pos_in = (direction == engine::fade_direction::diagonally_reversed) ? ImVec2(x + w, y) : ImVec2(x, y);
	const ImVec2 fade_pos_out = (direction == engine::fade_direction::vertically) ? ImVec2(x, y + h) :
		(direction == engine::fade_direction::horizontally) ? ImVec2(x + w, y) :
		(direction == engine::fade_direction::diagonally) ? ImVec2(x + w, y + h) :
		(direction == engine::fade_direction::diagonally_reversed) ? ImVec2(x, y + h) : ImVec2(0, 0);

	this->set_linear_alpha(
		vtx_buffer_start,
		vtx_buffer_end,
		fade_pos_in, fade_pos_out,
		col.transform(),
		col2.transform()
	);
}

void c_render::fade_rect_filled(float x, float y, float w, float h, hue::c_color col1, hue::c_color col2, engine::fade_direction direction, float rounding, engine::draw_flags flags)
{
	switch (direction)
	{
	case engine::vertically:
		this->rect_filled_multi_color(x,y,w,h, { col1, col1, col2, col2 }, rounding, flags);
		break;
	case engine::horizontally:
		this->rect_filled_multi_color(x, y, w, h, { col1, col2, col1, col2 }, rounding, flags);
		break;
	case engine::diagonally:
		this->rect_filled_multi_color(x, y, w, h, { col1, col2, col2, col1 }, rounding, flags);
		break;
	case engine::diagonally_reversed:
		this->rect_filled_multi_color(x, y, w, h, { col2, col1, col1, col2 }, rounding, flags);
		break;
	default:
		break;
	}
}

void c_render::radial_gradient(float x, float y, float w, float h, float radius, hue::c_color col1, hue::c_color col2)
{
	this->m_draw_list->_PathArcToFastEx(ImVec2(x, y), radius, 0, IM_DRAWLIST_ARCFAST_SAMPLE_MAX, 0);
	const int count = this->m_draw_list->_Path.Size - 1;

	unsigned int vtx_base = this->m_draw_list->_VtxCurrentIdx;
	this->m_draw_list->PrimReserve(count * 3, count + 1);

	const ImVec2 uv = this->m_draw_list->_Data->TexUvWhitePixel;
	this->m_draw_list->PrimWriteVtx(ImVec2(x, y), uv, col1.transform());
	for (int n = 0; n < count; n++)
		this->m_draw_list->PrimWriteVtx(this->m_draw_list->_Path[n], uv, col2.transform());

	for (int n = 0; n < count; n++)
	{
		this->m_draw_list->PrimWriteIdx((ImDrawIdx)(vtx_base));
		this->m_draw_list->PrimWriteIdx((ImDrawIdx)(vtx_base + 1 + n));
		this->m_draw_list->PrimWriteIdx((ImDrawIdx)(vtx_base + 1 + ((n + 1) % count)));
	}
	this->m_draw_list->_Path.Size = 0;
}


void c_render::enlarged_arrow(math::c_vector_2d pos, hue::c_color col, int dir, float scale)
{
	float thickness = ImMax(scale / 15.0f, 1.0f);
	scale -= thickness * 0.5f;
	pos += math::c_vector_2d(ImVec2(thickness * 0.25f, thickness * 0.25f).x, ImVec2(thickness * 0.25f, thickness * 0.25f).y);

	float third = scale / 3.0f;
	float bx = pos.x + third;
	float by = pos.y + scale - third * 0.5f;

	switch (dir) {
	case ImGuiDir_Down:
		this->m_draw_list->PathLineTo(ImVec2(bx - third, by - third));
		this->m_draw_list->PathLineTo(ImVec2(bx, by));
		this->m_draw_list->PathLineTo(ImVec2(bx + third, by - third));
		break;
	case ImGuiDir_Up:
		this->m_draw_list->PathLineTo(ImVec2(bx - third, by + third));
		this->m_draw_list->PathLineTo(ImVec2(bx, by));
		this->m_draw_list->PathLineTo(ImVec2(bx + third, by + third));
		break;
	case ImGuiDir_Left:
		this->m_draw_list->PathLineTo(ImVec2(bx + third, by - third));
		this->m_draw_list->PathLineTo(ImVec2(bx, by));
		this->m_draw_list->PathLineTo(ImVec2(bx + third, by + third));
		break;
	case ImGuiDir_Right:
		this->m_draw_list->PathLineTo(ImVec2(bx - third, by - third));
		this->m_draw_list->PathLineTo(ImVec2(bx, by));
		this->m_draw_list->PathLineTo(ImVec2(bx - third + third / 30, by + third));
		break;
	}

	this->m_draw_list->PathStroke(col.transform(), 0, thickness);
}

void c_render::radial_gradient_rect_filled(float x, float y, float w, float h, ImVec2 size, float rounding, hue::c_color col1, hue::c_color col2)
{
	ImDrawList* dl = this->m_draw_list;
	ImVec2 pos = ImVec2(x, y);
	ImVec2 max_pos = ImVec2(pos.x + size.x, pos.y + size.y);
	const ImU32 c1 = col1.transform();
	const ImU32 c2 = col2.transform();

	rounding = ImMin(rounding, ImMin(size.x, size.y) * 0.5f);

	auto lerp_color = [](ImU32 a, ImU32 b, float t) -> ImU32 {
		int ra = (a >> 0) & 0xFF, ga = (a >> 8) & 0xFF, ba = (a >> 16) & 0xFF, aa = (a >> 24) & 0xFF;
		int rb = (b >> 0) & 0xFF, gb = (b >> 8) & 0xFF, bb = (b >> 16) & 0xFF, ab = (b >> 24) & 0xFF;
		return IM_COL32(
			(int)(ra + (rb - ra) * t),
			(int)(ga + (gb - ga) * t),
			(int)(ba + (bb - ba) * t),
			(int)(aa + (ab - aa) * t)
		);
		};

	const int layers = 12;

	for (int i = 0; i < layers; ++i)
	{
		float t = (float)i / (float)(layers - 1);

		float offset_x = (size.x * 0.5f) * t;
		float offset_y = (size.y * 0.5f) * t;

		ImVec2 layer_pos = ImVec2(pos.x + offset_x, pos.y + offset_y);
		ImVec2 layer_max = ImVec2(max_pos.x - offset_x, max_pos.y - offset_y);
		ImVec2 layer_size = ImVec2(layer_max.x - layer_pos.x, layer_max.y - layer_pos.y);

		if (layer_size.x <= 0.0f || layer_size.y <= 0.0f)
			break;

		float layer_rounding = ImMin(rounding * (1.0f - t), ImMin(layer_size.x, layer_size.y) * 0.5f);

		dl->AddRectFilled(layer_pos, layer_max, lerp_color(c2, c1, t), layer_rounding);
	}
}

// hue::c_color top_r, hue::c_color top_l, hue::c_color bottom_r, hue::c_color bottom_l
void c_render::rect_filled_multi_color(float x, float y, float w, float h, engine::c_gradient_data gradient_data, float rounding, engine::draw_flags flags)
{
	auto fix_rect_corner_flags = [](engine::draw_flags rflags)
		{
			if ((rflags & engine::draw_flags_round_corners_mask) == 0)
				rflags |= engine::draw_flags_round_corners_all;
			return rflags;
		};

	auto p_min = ImVec2(x, y);
	auto p_max = (ImVec2(x, y) + ImVec2(w, h));

	flags = fix_rect_corner_flags(flags);
	rounding = ImMin(rounding, ImFabs(p_max.x - p_min.x) * (((flags & engine::draw_flags_round_corners_top) == engine::draw_flags_round_corners_top) || ((flags & engine::draw_flags_round_corners_bottom) == engine::draw_flags_round_corners_bottom) ? 0.5f : 1.0f) - 1.0f);
	rounding = ImMin(rounding, ImFabs(p_max.y - p_min.y) * (((flags & engine::draw_flags_round_corners_left) == engine::draw_flags_round_corners_left) || ((flags & engine::draw_flags_round_corners_right) == engine::draw_flags_round_corners_right) ? 0.5f : 1.0f) - 1.0f);

	if (rounding > 0.0f)
	{
		const int size_before = this->m_draw_list->VtxBuffer.Size;
		this->m_draw_list->AddRectFilled(p_min, p_max, IM_COL32_WHITE, rounding, flags);
		const int size_after = this->m_draw_list->VtxBuffer.Size;

		for (int i = size_before; i < size_after; i++)
		{
			ImDrawVert* vert = this->m_draw_list->VtxBuffer.Data + i;

			ImVec4 upr_left = ImGui::ColorConvertU32ToFloat4(gradient_data.m_top_l.transform());
			ImVec4 bot_left = ImGui::ColorConvertU32ToFloat4(gradient_data.m_bot_l.transform());
			ImVec4 up_right = ImGui::ColorConvertU32ToFloat4(gradient_data.m_top_r.transform());
			ImVec4 bot_right = ImGui::ColorConvertU32ToFloat4(gradient_data.m_bot_r.transform());

			float X = ImClamp((vert->pos.x - p_min.x) / (p_max.x - p_min.x), 0.0f, 1.0f);

			// 4 colors - 8 deltas

			float r1 = upr_left.x + (up_right.x - upr_left.x) * X;
			float r2 = bot_left.x + (bot_right.x - bot_left.x) * X;

			float g1 = upr_left.y + (up_right.y - upr_left.y) * X;
			float g2 = bot_left.y + (bot_right.y - bot_left.y) * X;

			float b1 = upr_left.z + (up_right.z - upr_left.z) * X;
			float b2 = bot_left.z + (bot_right.z - bot_left.z) * X;

			float a1 = upr_left.w + (up_right.w - upr_left.w) * X;
			float a2 = bot_left.w + (bot_right.w - bot_left.w) * X;


			float Y = ImClamp((vert->pos.y - p_min.y) / (p_max.y - p_min.y), 0.0f, 1.0f);
			float r = r1 + (r2 - r1) * Y;
			float g = g1 + (g2 - g1) * Y;
			float b = b1 + (b2 - b1) * Y;
			float a = a1 + (a2 - a1) * Y;
			ImVec4 RGBA(r, g, b, a);

			RGBA = RGBA * ImGui::ColorConvertU32ToFloat4(vert->col);

			vert->col = ImColor(RGBA);
		}
		return;
	}

	const ImVec2 uv = this->m_draw_list->_Data->TexUvWhitePixel;
	this->m_draw_list->PrimReserve(6, 4);
	this->m_draw_list->PrimWriteIdx((ImDrawIdx)(this->m_draw_list->_VtxCurrentIdx)); this->m_draw_list->PrimWriteIdx((ImDrawIdx)(this->m_draw_list->_VtxCurrentIdx + 1)); this->m_draw_list->PrimWriteIdx((ImDrawIdx)(this->m_draw_list->_VtxCurrentIdx + 2));
	this->m_draw_list->PrimWriteIdx((ImDrawIdx)(this->m_draw_list->_VtxCurrentIdx)); this->m_draw_list->PrimWriteIdx((ImDrawIdx)(this->m_draw_list->_VtxCurrentIdx + 2)); this->m_draw_list->PrimWriteIdx((ImDrawIdx)(this->m_draw_list->_VtxCurrentIdx + 3));


	this->m_draw_list->PrimWriteVtx(p_min, uv, gradient_data.m_top_l.transform());
	this->m_draw_list->PrimWriteVtx(ImVec2(p_max.x, p_min.y), uv, gradient_data.m_top_r.transform());
	this->m_draw_list->PrimWriteVtx(p_max, uv, gradient_data.m_bot_r.transform());
	this->m_draw_list->PrimWriteVtx(ImVec2(p_min.x, p_max.y), uv, gradient_data.m_bot_l.transform());
}


void c_render::check_mark(float x, float y, float size, hue::c_color col)
{
	float thickness = ImMax(size / 5.0f, 1.0f);
	size -= thickness * 0.5f;

	x += thickness * 0.25f;
	y += thickness * 0.25f;

	float third = size / 3.0f;
	float bx = x + third;
	float by = y + size - third * 0.5f;

	ImVec2 p0 = { bx - third,        by - third };
	ImVec2 p1 = { bx,                by };
	ImVec2 p2 = { bx + third * 2.f,  by - third * 2.f };

	float r = thickness * 0.5f;

	ImVec2 cp0 = ImVec2((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);
	ImVec2 cp1 = ImVec2((p1.x + p2.x) * 0.5f, (p1.y + p2.y) * 0.5f);

	this->m_draw_list->AddBezierCubic(p0, cp0, cp0, p1, col.transform(), thickness, 0);
	this->m_draw_list->AddBezierCubic(p1, cp1, cp1, p2, col.transform(), thickness, 0);

	this->m_draw_list->AddCircleFilled(p1, thickness * 0.5f, col.transform());

	this->m_draw_list->AddCircleFilled(p0, thickness * 0.5f, col.transform());
	this->m_draw_list->AddCircleFilled(p2, thickness * 0.5f, col.transform());
}

void c_render::push_clip(float x, float y, float w, float h)
{
	this->m_draw_list->PushClipRect(ImVec2(static_cast<float>(x), static_cast<float>(y)), ImVec2(static_cast<float>(x + w), static_cast<float>(y + h)), true);
}

void c_render::restore_clip()
{
	this->m_draw_list->PopClipRect();
}

void c_render::setup()
{
	if (!this->m_draw_list)
	{
		this->set_draw_list(ImGui::GetBackgroundDrawList());
		slog::log::info("[+] drawlist has been initialized.");
	}

	/* initialize fonts */
	ImGuiIO& io = ImGui::GetIO();

	ImFontConfig* cfg = new ImFontConfig();
	{
		cfg->FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_NoHinting;
		cfg->FontDataOwnedByAtlas = false;
		g_font->f_default.create((void*)roboto_regular, (int)roboto_regular_size, 18.f, cfg, framework_text_ranges(), "roboto_regular");
		merge_system_font(18.f, cfg->FontBuilderFlags);
		g_font->f_childs.create((void*)roboto_regular, (int)roboto_regular_size, 17.f, cfg, framework_text_ranges(), "f_childs");
		merge_system_font(17.f, cfg->FontBuilderFlags);
		g_font->f_bold.create((void*)roboto_bold, (int)roboto_bold_size, 17.f, cfg, framework_text_ranges(), "roboto_bold");
		merge_system_font(17.f, cfg->FontBuilderFlags);
	}

	cfg = new ImFontConfig();
	{
		static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
		cfg->FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_ForceAutoHint;
		cfg->FontDataOwnedByAtlas = false;

		g_font->f_icons.create((void*)FA_compressed_data, (int)FA_compressed_size, 14.0f, cfg, icons_ranges, "f_icons", true);
		g_font->f_icons_medium.create((void*)FA_compressed_data, (int)FA_compressed_size, 22.0f, cfg, icons_ranges, "f_icons_medium", true);
		g_font->f_icons_rs.create((void*)FA_compressed_data, (int)FA_compressed_size, 7.0f, cfg, icons_ranges, "f_icons_rs", true);
		g_font->f_oof_arrow.create((void*)FA_compressed_data, (int)FA_compressed_size, 70.0f, cfg, icons_ranges, "f_oof_arrow", true);

		static const ImWchar weapon_icons_ranges[] = { 0x25 , 0x66 , 0 };
		g_font->f_weapon_icons.create((void*)esp_icons, (int)esp_icons_size, 14.0f, cfg, weapon_icons_ranges, "f_weapon_icons", false);
	}

	//
	// c_font f_verdana{};
	// c_font f_smallest_pixel{};

	cfg = new ImFontConfig();
	{
		cfg->FontBuilderFlags |= ImGuiFreeTypeBuilderFlags_MonoHinting | ImGuiFreeTypeBuilderFlags_Monochrome;
		g_font->f_verdana.create("c:/windows/fonts/verdana.ttf", 13, cfg, NULL);
	}

	cfg = new ImFontConfig();
	{
		g_font->f_smallest_pixel.create((void*)_smallest_pixel, _smallest_pixel_size, 10, cfg, NULL, "sf_pro_bold");
	}

	g_font->build_fonts(io.Fonts);

	delete cfg;

	g_texture.preview.load_from_memory(preview_image, preview_image_size);
	g_texture.m_leaf_texture.load_from_file("leaf.png");

	slog::log::success("render engine has been initialized.");
}

bool c_font::create(const std::string& font_location, float size)
{
	ImGuiIO& io = ImGui::GetIO();

	this->m_handle = io.Fonts->AddFontFromFileTTF(font_location.c_str(), size);
	if (this->m_handle == nullptr) {
		slog::log::debug("failed to create font: {}, size: {}", font_location.c_str(), (int)size);
		return false;
	}

	this->m_size = this->measure("A");

	// finish
	slog::log::debug("created font: {}, size: {}", font_location.c_str(), (int)size);
	return true;
}

bool c_font::create(const std::string& font_location, float size, const ImFontConfig* font_template, const ImWchar* glyph)
{
	ImGuiIO& io = ImGui::GetIO();

	// init handler
	this->m_handle = io.Fonts->AddFontFromFileTTF(font_location.c_str(), size, font_template, glyph);
	if (this->m_handle == nullptr) {
		slog::log::debug("failed to create font: {}, size: {}", font_location.c_str(), (int)size);
		return false;
	}

	this->m_size = this->measure("A");

	// finish
	slog::log::debug("created font: {}, size: {}", font_location.c_str(), (int)size);
	return true;
}

bool c_font::create(void* data, int sizess, float size, std::string id, bool compresed)
{
	ImGuiIO& io = ImGui::GetIO();

	// font handler
	// check if the font has been initialized
	this->m_handle = compresed ? io.Fonts->AddFontFromMemoryCompressedTTF(data, sizess, size) : io.Fonts->AddFontFromMemoryTTF(data, sizess, size);
	if (this->m_handle == nullptr) {
		slog::log::debug("failed to create font: {}, size: {}, font_size_of: {}", id.c_str(), (int)size, (int)sizeof(data));
		return false;
	}

	this->m_size = this->measure("A");

	// finish
	slog::log::debug("created font: {}, size: {}", id.c_str(), (int)size);
	return true;
}

bool c_font::create(void* data, int font_size, float size, const ImFontConfig* font_template, const ImWchar* glyph, std::string id, bool compresed)
{
	ImGuiIO& io = ImGui::GetIO();

	// font handler
	// check if the font has been initialized
	this->m_handle = compresed ? io.Fonts->AddFontFromMemoryCompressedTTF(data, font_size, size, font_template, glyph) : io.Fonts->AddFontFromMemoryTTF(data, font_size, size, font_template, glyph);
	if (this->m_handle == nullptr) {
		slog::log::debug("failed to create font: {}, size: {}, font_size_of: {}", id.c_str(), (int)size, (int)sizeof(data));
		return false;
	}

	this->m_size = this->measure("A");

	// finish
	slog::log::debug("created font: {}, size: {}", id.c_str(), (int)size);
	return true;
}

math::c_vector_2d c_font::measure(const std::string& text)
{
	if (this->m_handle == nullptr)
		return math::c_vector_2d();

	auto wraper = this->m_handle->CalcTextSizeA(this->m_handle->FontSize, FLT_MAX, -1.0f, text.c_str());
	return math::c_vector_2d(wraper.x, wraper.y);
}

void c_font::string(float x, float y, std::string text, hue::c_color color)
{
	g_render->draw_list()->AddText(ImVec2(x, y), color.transform(), text.c_str());
}

void c_render::gradient(math::c_vector_2d pos, math::c_vector_2d size, hue::c_color color, hue::c_color color2, engine::fade_direction flags, float rounding, hue::c_color backround_helper, ImDrawFlags draw_flags)
{
	auto start = pos.transform();
	auto end = (pos + size).transform();

	if (rounding > 0)
	{
		this->m_draw_list->AddRectFilledMultiColorRounded(start, end, backround_helper.transform(),
			flags == engine::fade_direction::vertically ? color.transform() : color.transform(),
			flags == engine::fade_direction::vertically ? color2.transform() : color.transform(),
			flags == engine::fade_direction::vertically ? color2.transform() : color2.transform(),
			flags == engine::fade_direction::vertically ? color.transform() : color2.transform(),
			rounding, draw_flags);
	}
	else
	{
		this->m_draw_list->AddRectFilledMultiColor(
			start, end,
			flags == engine::fade_direction::vertically ? color.transform() : color.transform(),
			flags == engine::fade_direction::vertically ? color2.transform() : color.transform(),
			flags == engine::fade_direction::vertically ? color2.transform() : color2.transform(),
			flags == engine::fade_direction::vertically ? color.transform() : color2.transform()
		);
	}
}

void c_render::gradient(float x, float y, float w, float h, hue::c_color color, hue::c_color color2, engine::fade_direction flags, float rounding, hue::c_color backround_helper, ImDrawFlags draw_flags)
{
	if (flags == engine::fade_direction::vertically) {
		if (rounding != 0) {
			this->m_draw_list->AddRectFilledMultiColorRounded(ImVec2(x, y), ImVec2(x + w, y + h), backround_helper.transform(),
				color.transform(), color2.transform(), color2.transform(), color.transform(), rounding, draw_flags);
		}
		else {
			this->m_draw_list->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + w, y + h),
				color.transform(), color2.transform(), color2.transform(), color.transform());
		}
	}
	else if (flags == engine::fade_direction::horizontally) {
		if (rounding != 0) {
			this->m_draw_list->AddRectFilledMultiColorRounded(ImVec2(x, y), ImVec2(x + w, y + h), backround_helper.transform(),
				color.transform(), color.transform(), color2.transform(), color2.transform(), rounding, draw_flags);
		}
		else {
			this->m_draw_list->AddRectFilledMultiColor(ImVec2(x, y), ImVec2(x + w, y + h),
				color.transform(), color.transform(), color2.transform(), color2.transform());
		}
	}
}

void c_render::dashed_rect(float x, float y, float w, float h, hue::c_color col, float rounding, float thickness, float dash_length, float gap_length)
{
	float fx = static_cast<float>(x);
	float fy = static_cast<float>(y);
	float fw = static_cast<float>(w);
	float fh = static_cast<float>(h);

	float r = std::min(rounding, std::min(fw * 0.5f, fh * 0.5f));

	std::vector<ImVec2> path;
	const int arc_segments = 24;

	auto add_arc = [&](float cx, float cy, float angle_start, float angle_end) {
		for (int i = 0; i <= arc_segments; i++) {
			float a = angle_start + (angle_end - angle_start) * (float)i / (float)arc_segments;
			path.push_back({ cx + cosf(a) * r, cy + sinf(a) * r });
		}
		};

	add_arc(fx + r, fy + r, -IM_PI, -IM_PI * 0.5f);
	add_arc(fx + fw - r, fy + r, -IM_PI * 0.5f, 0.0f);
	add_arc(fx + fw - r, fy + fh - r, 0.0f, IM_PI * 0.5f);
	add_arc(fx + r, fy + fh - r, IM_PI * 0.5f, IM_PI);

	if (!path.empty())
		path.push_back(path[0]);

	if (path.size() < 2)
		return;

	std::vector<float> dist;
	dist.reserve(path.size());
	dist.push_back(0.0f);

	for (size_t i = 1; i < path.size(); i++)
	{
		float dx = path[i].x - path[i - 1].x;
		float dy = path[i].y - path[i - 1].y;
		dist.push_back(dist.back() + sqrtf(dx * dx + dy * dy));
	}

	float total_length = dist.back();
	if (total_length < 1.0f)
		return;

	float cycle = dash_length + gap_length;
	int num_cycles = std::max(1, (int)roundf(total_length / cycle));
	float adjusted_cycle = total_length / (float)num_cycles;
	float adjusted_dash = adjusted_cycle * (dash_length / cycle);

	size_t hint = 0;
	auto point_at = [&](float d) -> ImVec2 {
		d = fmodf(d, total_length);
		if (d < 0.0f) d += total_length;

		size_t lo = 0, hi = dist.size() - 1;
		while (lo + 1 < hi) {
			size_t mid = (lo + hi) / 2;
			if (dist[mid] <= d)
				lo = mid;
			else
				hi = mid;
		}

		float seg_len = dist[hi] - dist[lo];
		if (seg_len < 0.0001f)
			return path[lo];

		float t = (d - dist[lo]) / seg_len;
		return {
			path[lo].x + (path[hi].x - path[lo].x) * t,
			path[lo].y + (path[hi].y - path[lo].y) * t
		};
		};

	ImU32 color = col.transform();
	const float step = 1.5f;

	float dash_offset = adjusted_cycle * 0.5f;
	for (int i = 0; i < num_cycles; i++)
	{

		float dash_start = (float)i * adjusted_cycle + dash_offset;
		float dash_end = dash_start + adjusted_dash;

		hint = 0;
		ImVec2 prev = point_at(dash_start);

		for (float d = dash_start + step; d < dash_end; d += step)
		{
			ImVec2 cur = point_at(d);
			this->m_draw_list->AddLine(prev, cur, color, thickness);
			prev = cur;
		}

		ImVec2 end = point_at(dash_end);
		this->m_draw_list->AddLine(prev, end, color, thickness);
	}
}


#define STB_IMAGE_IMPLEMENTATION
#include "stb/stb_image.hh"

void c_texture::load_from_file(const char* filename)
{
	int image_width = 0;
	int image_height = 0;

	unsigned char* image_data = stbi_load(filename, &image_width, &image_height, NULL, 4);
	if (image_data == NULL)
	{
		this->m_loaded = false;

		slog::log::error("failed to load texture: {}", filename);

		return;
	}

	for (int i = 0; i < image_width * image_height; i++) {
		unsigned char r = image_data[i * 4 + 0];
		unsigned char g = image_data[i * 4 + 1];
		unsigned char b = image_data[i * 4 + 2];
		// if pixel is near-white, make it fully transparent
		if (r > 220 && g > 220 && b > 220)
			image_data[i * 4 + 3] = 0;
		// soft edge: semi-transparent for slightly off-white
		else if (r > 180 && g > 180 && b > 180) {
			float whiteness = (r + g + b) / 3.f / 255.f;
			image_data[i * 4 + 3] = (unsigned char)((1.f - whiteness) * 2.f * 255.f);
		}
	}

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = image_width;
	desc.Height = image_height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;
	ID3D11Texture2D* pTexture = NULL;
	D3D11_SUBRESOURCE_DATA subResource;
	subResource.pSysMem = image_data;
	subResource.SysMemPitch = desc.Width * 4;
	subResource.SysMemSlicePitch = 0;

	core::g_overlay->m_device->CreateTexture2D(&desc, &subResource, &pTexture);

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = desc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;

	core::g_overlay->m_device->CreateShaderResourceView(pTexture, &srvDesc, &this->m_srv);

	pTexture->Release();

	this->m_width = image_width;
	this->m_height = image_height;

	stbi_image_free(image_data);

	this->m_loaded = true;
}

void c_texture::load_from_memory(unsigned char* Memory, UINT size)
{
	int image_width = 0;
	int image_height = 0;

	unsigned char* image_data = stbi_load_from_memory(Memory, size, &image_width, &image_height, NULL, 4);
	if (image_data == NULL)
	{
		this->m_loaded = false;
		slog::log::error("failed to load texture");
		return;
	}

	D3D11_TEXTURE2D_DESC desc;
	ZeroMemory(&desc, sizeof(desc));
	desc.Width = image_width;
	desc.Height = image_height;
	desc.MipLevels = 1;
	desc.ArraySize = 1;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.SampleDesc.Count = 1;
	desc.Usage = D3D11_USAGE_DEFAULT;
	desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
	desc.CPUAccessFlags = 0;

	D3D11_SUBRESOURCE_DATA subResource;
	subResource.pSysMem = image_data;
	subResource.SysMemPitch = desc.Width * 4;
	subResource.SysMemSlicePitch = 0;

	ID3D11Texture2D* pTexture = NULL;
	HRESULT hr = core::g_overlay->m_device->CreateTexture2D(&desc, &subResource, &pTexture);
	if (FAILED(hr) || !pTexture)
	{
		stbi_image_free(image_data);
		this->m_loaded = false;
		slog::log::error("failed to create texture2d");
		return;
	}

	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc;
	ZeroMemory(&srvDesc, sizeof(srvDesc));
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MipLevels = desc.MipLevels;
	srvDesc.Texture2D.MostDetailedMip = 0;

	hr = core::g_overlay->m_device->CreateShaderResourceView(pTexture, &srvDesc, &this->m_srv);
	pTexture->Release();

	if (FAILED(hr))
	{
		stbi_image_free(image_data);
		this->m_loaded = false;
		slog::log::error("failed to create shader resource view");
		return;
	}

	this->m_width = image_width;
	this->m_height = image_height;

	stbi_image_free(image_data);
	this->m_loaded = true;
}






