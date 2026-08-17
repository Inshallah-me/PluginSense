#include "../../../includes.hh"

void framework::c_search::debug_database()
{
	if (this->m_database.empty())
		return;

	// print database
	//for (auto data : this->m_database)
	//{
	//	slog::log::info("control: {}, tab name: {}, tab index: {}", data.m_copied_controls->m_label, data.tab_name, data.tab_index);
	//}
}

std::string to_upper(std::string s) {
	std::transform(s.begin(), s.end(), s.begin(),
		[](unsigned char c) { return toupper(c); });
	return s;
}

static bool contains_non_ascii(const std::string& text) {
	for (unsigned char c : text) {
		if (c >= 0x80)
			return true;
	}
	return false;
}

static void append_utf8(std::string& out, ImWchar c) {
	if (c < 32)
		return;

	const auto codepoint = static_cast<std::uint32_t>(c);
	char buf[5]{};
	if (codepoint < 0x80) {
		buf[0] = static_cast<char>(codepoint);
	}
	else if (codepoint < 0x800) {
		buf[0] = static_cast<char>(0xC0 | (codepoint >> 6));
		buf[1] = static_cast<char>(0x80 | (codepoint & 0x3F));
	}
	else if (codepoint < 0x10000) {
		buf[0] = static_cast<char>(0xE0 | (codepoint >> 12));
		buf[1] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
		buf[2] = static_cast<char>(0x80 | (codepoint & 0x3F));
	}
	else {
		buf[0] = static_cast<char>(0xF0 | (codepoint >> 18));
		buf[1] = static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
		buf[2] = static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
		buf[3] = static_cast<char>(0x80 | (codepoint & 0x3F));
	}

	out += buf;
}

static void append_clipboard_text(std::string& out) {
	const char* clip = ImGui::GetClipboardText();
	if (!clip)
		return;

	for (const char* p = clip; *p; ++p) {
		if (*p == '\r')
			continue;
		out.push_back(*p == '\n' ? ' ' : *p);
	}
}

static void erase_last_utf8(std::string& text) {
	if (text.empty())
		return;

	size_t pos = text.size() - 1;
	while (pos > 0 && (static_cast<unsigned char>(text[pos]) & 0xC0) == 0x80)
		--pos;

	text.erase(pos);
}

std::unordered_map<size_t, float> m_result_anims;

void framework::c_search::paint_database()
{
	// gheto pos
	auto pos = this->m_pos;
	auto size = math::c_vector_2d(200, 80);

	// search
	auto pos_search = pos + math::c_vector_2d(10, 45);
	auto pos_size = math::c_vector_2d(size.x - 20, 25);

	auto opacity = g_search.m_search_anim;

	// input
	{
		math::c_rect bounding = math::c_rect(pos_search, pos_size);

		if (g_input->mouse_in_region(bounding.pos(), bounding.size()) && g_input->clicked(input::mouse_buttons::left)) {
			this->m_focused = true;
		}
		else if (!g_input->mouse_in_region(bounding.pos(), bounding.size()) && g_input->clicked(input::mouse_buttons::left)) {
			this->m_focused = false;			
		}

		this->m_search_focusing = utils::builder::create_animation_ctx("search_focusing", this->m_focused, 0.5f);

		if (this->m_focused)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_Enter) ||
				ImGui::IsKeyPressed(ImGuiKey_Delete)) {
				this->m_focused = false;
			}

			if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
				append_clipboard_text(this->m_search_text);
			}

			if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && !this->m_search_text.empty()) {
				if (contains_non_ascii(this->m_search_text)) {
					erase_last_utf8(this->m_search_text);
					m_char_anims.clear();
					m_char_removing.clear();
					m_ghost_chars.clear();
				}
				else {
					int last = (int)this->m_search_text.size() - 1;

					// snapshot the dying char
					if (last < (int)m_char_anims.size()) {
						ghost_char_t ghost{};
						ghost.m_glyph = this->m_search_text[last];
						ghost.m_anim = m_char_anims[last]; // inherit current anim value

						// calculate its x at time of removal
						float gx = pos_search.x + 8.f;
						for (int c = 0; c < last; c++) {
							char tmp[2] = { this->m_search_text[c], '\0' };
							gx += g_font->f_childs.measure(tmp).x;
						}
						ghost.m_x = gx;

						m_ghost_chars.push_back(ghost);
						m_char_anims.erase(m_char_anims.begin() + last);
						m_char_removing.erase(m_char_removing.begin() + last);
					}

					this->m_search_text = this->m_search_text.substr(0, last);
				}
			}

			for (ImWchar c : ImGui::GetIO().InputQueueCharacters)
				append_utf8(this->m_search_text, c);
		}
	}

	if (!this->m_search_text.empty())
	{
		float result_y = 0.f;
		std::string search = this->m_search_text;
		std::transform(search.begin(), search.end(), search.begin(), ::tolower);

		float dt = ImGui::GetIO().DeltaTime;
		float speed = 10.f;

		for (auto& entry : this->m_database)
		{
			std::string label = entry.m_copied_controls->m_label;
			std::transform(label.begin(), label.end(), label.begin(), ::tolower);

			size_t hash = std::hash<std::string>()(label);
			bool   is_match = search.size() >= 2 && label.find(search) != std::string::npos;

			//if (entry.m_copied_controls->m_type == element_type::dropdown)
			//	slog::log::success("dropdown: {}, search: {}, label: {}, is_match: {}", entry.m_copied_controls->m_label, search, label, is_match);
			//
			//slog::log::warn("search: {}, label: {}, is_match: {}", search, label, is_match);

			float& anim = m_result_anims[hash];
			float  target = is_match ? 1.f : 0.f;
			anim += (target - anim) * dt * speed;
		}

		for (auto& entry : this->m_database)
		{
			std::string label = entry.m_copied_controls->m_label;
			std::transform(label.begin(), label.end(), label.begin(), ::tolower);

			size_t hash = std::hash<std::string>()(label);
			float  t = m_result_anims[hash];

			if (t <= 0.01f)
				continue;

			float eased = t * t * (3.f - 2.f * t);

			entry.m_copied_controls->m_visible = true;
			entry.m_copied_controls->m_search_override = true;
			entry.m_copied_controls->m_pos = pos + math::c_vector_2d(10, 80 + result_y);
			
			entry.m_backup_x = entry.m_copied_controls->m_child_size;
			
			entry.m_copied_controls->set_layer(engine::render_layer::middle);
			entry.m_copied_controls->set_parent("search_bar");

			// capture verts
			int vtx_start = g_render->draw_list()->VtxBuffer.Size;

			entry.m_copied_controls->m_child_size = size.x - 20.f;

			entry.m_copied_controls->input();

			if (entry.m_copied_controls->m_call_stacks)
				entry.m_copied_controls->m_call_stacks();

			entry.m_copied_controls->draw();

			int vtx_end = g_render->draw_list()->VtxBuffer.Size;

			ImDrawVert* verts = g_render->draw_list()->VtxBuffer.Data;
			for (int v = vtx_start; v < vtx_end; v++)
			{

				int a = (int)(((verts[v].col >> IM_COL32_A_SHIFT) & 0xFF) * eased);
				verts[v].col = (verts[v].col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
			}

			float contribution = (entry.m_copied_controls->m_size.y + 8.f) * eased;
			result_y += contribution;
			size += math::c_vector_2d(0, contribution);

			entry.m_copied_controls->m_search_override = false;
			entry.m_copied_controls->m_child_size = entry.m_backup_x;
		}
	}
	else
	{
		float dt = ImGui::GetIO().DeltaTime;
		float speed = 10.f;
		for (auto& [hash, anim] : m_result_anims)
			anim += (0.f - anim) * dt * speed;
	}

	float t = opacity.val();
	float eased = t * t * (3.f - 2.f * t);

	float origin_y = pos.y;
	float origin_x = pos.x;

	// middle layer doesn't pass ignore_clipping so it stays on m_draw_list
	ImDrawList* dl = g_render->draw_list();
	int vtx_start = dl->VtxBuffer.Size;

	g_render->rect_shadow(pos.x, pos.y, size.x, size.y, g_style->m_window_shadow.modulate(opacity.limit(0.5f).val()), 10.f, 10.f);
	opacity.restore();
	g_render->rect_filled(pos.x, pos.y, size.x, size.y, g_style->m_window_background.modulate(opacity.val()), 10.f);
	g_render->rect_filled(pos.x, pos.y, size.x, 35, g_style->m_window_bars.modulate(opacity.val()), 10.f, engine::draw_flags_::draw_flags_round_corners_top);
	g_render->gradient(pos.x, pos.y + 35, size.x, 10, g_style->m_window_shadow.modulate(opacity.limit(0.2f).val()), g_style->m_window_shadow.modulate(opacity.limit(0.1f).val()).with_alpha(0), engine::fade_direction::horizontally);
	opacity.restore();
	g_font->f_default.text(pos.x + 10, pos.y + 8, "Search results", g_style->m_text.modulate(opacity.limit(0.5f).val())/*, engine::modifiers::font_flags::drop_shadow*/);
	opacity.restore();

	g_render->rect_shadow(pos_search.x, pos_search.y, pos_size.x, pos_size.y, g_style->m_window_shadow.modulate(opacity.limit(0.5f).val()).lerp(g_style->m_accent.modulate(opacity.limit(0.5f).val()), this->m_search_focusing.val()), 10.f, 5.f);
	opacity.restore();
	g_render->rect_filled(pos_search.x, pos_search.y, pos_size.x, pos_size.y, g_style->m_element_base.modulate(opacity.val()), 5.f);


	if (this->m_focused)
	{
		if (contains_non_ascii(this->m_search_text))
		{
			m_char_anims.clear();
			m_char_removing.clear();
			m_ghost_chars.clear();

			g_font->f_childs.text(pos_search.x + 8, pos_search.y + 3, this->m_search_text, g_style->m_text.modulate(opacity.limit(0.5f).val()));
			const float cursor_x = pos_search.x + 8 + g_font->f_childs.measure(this->m_search_text).x;

			if (this->m_search_focusing.val() >= 0.01f) {
				const uint64_t now_ms = GetTickCount64();
				if (now_ms >= (uint64_t)blink)
					blink = (float)(now_ms + 800);

				if (now_ms > (uint64_t)(blink - 400)) {
					const auto cursor_col = g_style->m_text.modulate(this->m_search_focusing.limit(0.5f).val()).lerp(g_style->m_accent.modulate(this->m_search_focusing.limit(0.5f).val()), this->m_search_focusing.val());
					g_font->f_childs.text(cursor_x, pos_search.y + 3, "|", cursor_col);
				}
			}
		}
		else
		{
		while (m_char_anims.size() < this->m_search_text.size())
		{
			m_char_anims.push_back(0.f);
			m_char_removing.push_back(false);
		}

		float dt = ImGui::GetIO().DeltaTime;
		float speed = 12.f;

		float cursor_x = pos_search.x + 8.f;
		float base_y = pos_search.y + 3.f;

		for (int i = 0; i < (int)m_ghost_chars.size(); i++)
		{
			auto& ghost = m_ghost_chars[i];
			ghost.m_anim += (0.f - ghost.m_anim) * dt * speed;

			if (ghost.m_anim < 0.01f) {
				m_ghost_chars.erase(m_ghost_chars.begin() + i);
				i--;
				continue;
			}

			float t = ghost.m_anim;
			float eased = t * t * (3.f - 2.f * t);

			char buf[2] = { ghost.m_glyph, '\0' };
			auto char_size = g_font->f_childs.measure(buf);
			float char_center_x = ghost.m_x + char_size.x * 0.5f;
			float char_center_y = base_y + char_size.y * 0.5f;

			int vtx_start = g_render->draw_list()->VtxBuffer.Size;

			g_font->f_childs.text(ghost.m_x, base_y, buf,
				g_style->m_text.modulate(opacity.limit(0.5f).val()));
			opacity.restore();

			int vtx_end = g_render->draw_list()->VtxBuffer.Size;

			ImDrawVert* verts = g_render->draw_list()->VtxBuffer.Data;
			for (int v = vtx_start; v < vtx_end; v++)
			{
				verts[v].pos.x = char_center_x + (verts[v].pos.x - char_center_x) * eased;
				verts[v].pos.y = char_center_y + (verts[v].pos.y - char_center_y) * eased;

				int a = (int)(((verts[v].col >> IM_COL32_A_SHIFT) & 0xFF) * eased);
				verts[v].col = (verts[v].col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
			}
		}

		for (int i = 0; i < (int)m_char_anims.size(); i++)
		{
			bool is_active = i < (int)this->m_search_text.size();

			float target = is_active ? 1.f : 0.f;
			m_char_anims[i] += (target - m_char_anims[i]) * dt * speed;

			if (!is_active && m_char_anims[i] < 0.01f)
			{
				m_char_anims.erase(m_char_anims.begin() + i);
				m_char_removing.erase(m_char_removing.begin() + i);
				i--;
				continue;
			}

			float t = m_char_anims[i];
			float eased = t * t * (3.f - 2.f * t);

			char buf[2] = { is_active ? this->m_search_text[i] : ' ', '\0' };
			auto char_size = g_font->f_childs.measure(buf);
			float char_center_x = cursor_x + char_size.x * 0.5f;
			float char_center_y = base_y + char_size.y * 0.5f;

			int vtx_start = g_render->draw_list()->VtxBuffer.Size;

			g_font->f_childs.text(cursor_x, base_y, buf,
				g_style->m_text.modulate(opacity.limit(0.5f).val()));
			opacity.restore();

			int vtx_end = g_render->draw_list()->VtxBuffer.Size;

			ImDrawVert* verts = g_render->draw_list()->VtxBuffer.Data;
			for (int v = vtx_start; v < vtx_end; v++)
			{
				verts[v].pos.x = char_center_x + (verts[v].pos.x - char_center_x) * eased;
				verts[v].pos.y = char_center_y + (verts[v].pos.y - char_center_y) * eased;

				int a = (int)(((verts[v].col >> IM_COL32_A_SHIFT) & 0xFF) * eased);
				verts[v].col = (verts[v].col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
			}

			cursor_x += char_size.x;
		}

		std::string to_show{};
		if (this->m_search_focusing.val() >= 0.01f) {
			const uint64_t now_ms = GetTickCount64();
			if (now_ms >= (uint64_t)blink)
				blink = (float)(now_ms + 800);

			if (now_ms > (uint64_t)(blink - 400)) {
				const auto cursor_col = g_style->m_text.modulate(this->m_search_focusing.limit(0.5f).val()).lerp(g_style->m_accent.modulate(this->m_search_focusing.limit(0.5f).val()), this->m_search_focusing.val());
				g_font->f_childs.text(cursor_x, pos_search.y + 3, "|", cursor_col);
			}
		}
		}
	}
	else
	{
		g_font->f_childs.text(pos_search.x + 8, pos_search.y + 3, this->m_search_text.empty() && !this->m_focused ? "Search..." : this->m_search_text, g_style->m_text.modulate(opacity.limit(0.5f).val()));
	}

	int vtx_end = dl->VtxBuffer.Size;

	ImDrawVert* verts = dl->VtxBuffer.Data;
	for (int v = vtx_start; v < vtx_end; v++)
	{
		verts[v].pos.x = origin_x + (verts[v].pos.x - origin_x) * eased;
		verts[v].pos.y = origin_y + (verts[v].pos.y - origin_y) * eased;

		int a = (int)(((verts[v].col >> IM_COL32_A_SHIFT) & 0xFF) * eased);
		verts[v].col = (verts[v].col & ~IM_COL32_A_MASK) | (a << IM_COL32_A_SHIFT);
	}

	m_backup_size = size;
}

void framework::c_search::add_to_database(std::shared_ptr<c_base_element> control, std::string tab_name, int tab_index)
{
	this->m_database.push_back({ control, 0, tab_name, tab_index });
}

void framework::c_search::cleanup()
{
	this->m_search_text.clear();
	this->m_focused = false;
}
