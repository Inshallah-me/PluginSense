#include "../../../includes.hh"

namespace framework
{
    static constexpr float k_fade_dur = 0.15f;
    static constexpr float k_offset_px = 3.0f;

    static float ease_out_cubic(float t) {
        t = std::clamp(t, 0.f, 1.f);
        return 1.f - std::pow(1.f - t, 3.f);
    }

    static std::string to_upper(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
            [](unsigned char c) { return toupper(c); });
        return s;
    }

    static float ease_in_cubic(float t) {
        t = std::clamp(t, 0.f, 1.f);
        return t * t * t;
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

static float char_x_at_index(const std::string& text, int idx, float base_x)
{
    float x = base_x;
    for (int i = 0; i < idx && i < (int)text.size(); i++) {
        char tmp[2] = { text[i], '\0' };
        x += g_font->f_childs.measure(tmp).x;
    }
    return x;
}

static int next_char_boundary(const std::string& text, int pos)
{
    if (pos >= (int)text.size()) return (int)text.size();
    int n = 1;
    if ((unsigned char)text[pos] >= 0xC0) {
        while (pos + n < (int)text.size() && ((unsigned char)text[pos + n] & 0xC0) == 0x80)
            n++;
    }
    return pos + n;
}

static int prev_char_boundary(const std::string& text, int pos)
{
    if (pos <= 0) return 0;
    int p = pos - 1;
    while (p > 0 && ((unsigned char)text[p] & 0xC0) == 0x80)
        p--;
    return p;
}

static float measure_cursor_x(const std::string& text, int byte_pos, float base_x)
{
    if (contains_non_ascii(text))
        return base_x + g_font->f_childs.measure(text.substr(0, byte_pos)).x;
    return char_x_at_index(text, byte_pos, base_x);
}

    c_text_input::c_text_input(std::string label, std::string* var, bool hide_label)
        : m_var(var)
    {
        m_label = std::move(label);
        m_hide_label = hide_label;
        m_size = { 0, (m_hide_label ? 0 : g_font->f_childs.measure(m_label).y) + 30 };
        m_type = element_type::text_input;
        m_focus_priority = focus_priority::interactive;
        m_parent_width = m_child_size;
        m_cursor_pos = var ? (int)var->size() : 0;
    }

    void c_text_input::draw()
    {
        animations::m_textinput_opacity = utils::builder::create_animation_ctx(m_parent + m_label, m_visible && g_ctx->m_open, 0.5f);
        animations::m_textinput_value = utils::builder::create_animation_ctx(m_parent + m_label + "#m_textinput_value", m_visible && g_ctx->top_focus() == this && g_ctx->m_open, 0.5f);
        animations::m_textinput_hover = utils::builder::create_animation_ctx(m_parent + m_label + "#m_textinput_hover", m_visible && g_ctx->m_hovered == this, 0.5f);

        float target_opacity = 0.2f;
        if (animations::m_textinput_value.val() > 0.f)
            target_opacity = 0.2f + (0.6f * animations::m_textinput_value.val());
        else if (animations::m_textinput_hover.val() > 0.f)
            target_opacity = 0.2f + (0.2f * animations::m_textinput_hover.val());

        static std::unordered_map<std::string, float> smooth_opacity_cache;
        float& smooth_opacity = smooth_opacity_cache[m_parent + m_label + "#smooth_opacity"];
        smooth_opacity += (target_opacity - smooth_opacity) * 0.3f;

        const float final_opacity = animations::m_checkbox_opacity.val() * smooth_opacity;
        auto position = m_hide_label ? math::c_vector_2d(0, 0) : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);


        g_render->use_layer(m_layer, [&]()
            {
                if (!m_hide_label)
                    g_font->f_childs.text(m_pos.x, m_pos.y - 0.5f, m_label, g_style->m_text.modulate(final_opacity));

                const auto box_pos = m_pos + position;
                const auto text_pos = box_pos + math::c_vector_2d(8.f + m_scroll_offset, 3.f);
                auto shadow_col = g_style->m_window_shadow.modulate(animations::m_window_opacity.limit(0.3f).val())
                    .lerp(g_style->m_accent.modulate(animations::m_window_opacity.limit(0.5f).val()), animations::m_textinput_value.val());

                g_render->rect_shadow(box_pos.x, box_pos.y, m_child_size, 25.f, shadow_col, 8.f, 3.f);
                animations::m_window_opacity.restore();
                g_render->rect_filled(box_pos.x, box_pos.y, m_child_size, 25.f, g_style->m_element_base.modulate(animations::m_window_opacity.val()), 3.f);

                g_render->push_clip(box_pos.x + 8.f, box_pos.y, m_child_size - 16.f, 25.f);

                if (g_ctx->is_focused(this))
                {
                    if (contains_non_ascii(*this->m_var))
                    {
                        m_char_anims.clear();
                        m_char_removing.clear();
                        m_ghost_chars.clear();

                        g_font->f_childs.text(text_pos.x, text_pos.y, *this->m_var, g_style->m_text.modulate(animations::m_window_opacity.limit(0.5f).val()));
                        const float cursor_x = measure_cursor_x(*this->m_var, m_cursor_pos, text_pos.x);

                        if (animations::m_textinput_value.val() >= 0.01f) {
                            const uint64_t now_ms = GetTickCount64();
                            if (now_ms >= (uint64_t)blink)
                                blink = (float)(now_ms + 800);

                            if (now_ms > (uint64_t)(blink - 400)) {
                                const auto cursor_col = g_style->m_accent.modulate(animations::m_window_opacity.limit(0.8f).val());
                                const float cursor_h = g_font->f_childs.measure("0").y - 2.f;
                                g_render->rect_filled(cursor_x, text_pos.y + 2.f, 1.f, cursor_h, cursor_col, 0.f);
                            }
                        }
                    }
                    else
                    {
                    while (m_char_anims.size() < this->m_var->size())
                    {
                        m_char_anims.push_back(0.f);
                        m_char_removing.push_back(false);
                    }

                    float dt = ImGui::GetIO().DeltaTime;
                    float speed = 12.f;

                    float cursor_x = text_pos.x;
                    float base_y = text_pos.y;

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
                            g_style->m_text.modulate(animations::m_window_opacity.limit(0.5f).val()));
                        animations::m_window_opacity.restore();

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
                        bool is_active = i < (int)this->m_var->size();

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

                        char buf[2] = { is_active ? (*this->m_var)[i] : ' ', '\0' };
                        auto char_size = g_font->f_childs.measure(buf);
                        float char_center_x = cursor_x + char_size.x * 0.5f;
                        float char_center_y = base_y + char_size.y * 0.5f;

                        int vtx_start = g_render->draw_list()->VtxBuffer.Size;

                        g_font->f_childs.text(cursor_x, base_y, buf,
                            g_style->m_text.modulate(animations::m_window_opacity.limit(0.5f).val()));
                        animations::m_window_opacity.restore();

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

                    if (animations::m_textinput_value.val() >= 0.01f) {
                        const uint64_t now_ms = GetTickCount64();
                        if (now_ms >= (uint64_t)blink)
                            blink = (float)(now_ms + 800);

                        if (now_ms > (uint64_t)(blink - 400)) {
                            const auto cursor_col = g_style->m_accent.modulate(animations::m_window_opacity.limit(0.8f).val());
                            const float cur_x = measure_cursor_x(*this->m_var, m_cursor_pos, text_pos.x);
                            const float cursor_h = g_font->f_childs.measure("0").y - 2.f;
                            g_render->rect_filled(cur_x, text_pos.y + 2.f, 1.f, cursor_h, cursor_col, 0.f);
                        }
                    }
                    }
                }
                else
                {
                    g_font->f_childs.text(text_pos.x, text_pos.y, this->m_var->empty() && !g_ctx->is_focused(this) ? "" : *this->m_var, g_style->m_text.modulate(animations::m_window_opacity.limit(0.5f).val()));
                }

                g_render->restore_clip();
            });

       
    }

    void c_text_input::input()
    {
        auto position = m_hide_label
            ? math::c_vector_2d(0, 0)
            : math::c_vector_2d(0, g_font->f_childs.measure(m_label).y + 5);

        math::c_rect bounding = math::c_rect(
            m_pos + math::c_vector_2d(0, position.y),
            math::c_vector_2d(m_child_size, 20.f));

        if (!g_ctx->can_interact(this, m_focus_priority))
            return;

       //if (g_ctx->m_focus_took != nullptr && g_ctx->m_focus_took != this)
       //    return;

        g_ctx->m_hovered = g_input->mouse_in_region(bounding.pos(), bounding.size()) ? this : nullptr;

        if (g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == this && !g_ctx->m_click_consumed)
        {
            m_cursor_pos = (int)this->m_var->size();
            g_ctx->push_focus(this, m_focus_priority);
            g_ctx->m_click_consumed = true;
        }
      //     g_ctx->m_focused = this;
      //
      // if (g_ctx->m_focused != this)
      //     return;

        if (g_ctx->is_focused(this) && g_input->clicked(input::mouse_buttons::left) && g_ctx->m_hovered == nullptr && !g_ctx->m_click_consumed) {
            g_ctx->pop_focus(this);
            g_ctx->m_click_consumed = true;
            return;
        }

        if (!g_ctx->is_focused(this))
            return;

        if (ImGui::IsKeyPressed(ImGuiKey_Escape) || ImGui::IsKeyPressed(ImGuiKey_Enter) ||
            ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            g_ctx->pop_focus(this);
        }

        if (m_cursor_pos < 0) m_cursor_pos = 0;
        if (m_cursor_pos > (int)this->m_var->size()) m_cursor_pos = (int)this->m_var->size();

        if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) && m_cursor_pos > 0)
            m_cursor_pos = prev_char_boundary(*this->m_var, m_cursor_pos);
        if (ImGui::IsKeyPressed(ImGuiKey_RightArrow) && m_cursor_pos < (int)this->m_var->size())
            m_cursor_pos = next_char_boundary(*this->m_var, m_cursor_pos);

        // Scroll to keep cursor visible
        {
            const float visible_w = m_child_size - 16.f;
            const float cursor_x = measure_cursor_x(*this->m_var, m_cursor_pos, (this->m_pos + position).x + 8.f + m_scroll_offset);
            const float left_edge = (this->m_pos + position).x + 8.f;
            const float right_edge = left_edge + visible_w;

            if (cursor_x < left_edge)
                m_scroll_offset += (left_edge - cursor_x) + 20.f;
            else if (cursor_x > right_edge)
                m_scroll_offset -= (cursor_x - right_edge) + 20.f;
            m_scroll_offset = std::min(m_scroll_offset, 0.f);
        }

        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_V, false)) {
            append_clipboard_text(*this->m_var);
            m_cursor_pos = (int)this->m_var->size();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Backspace) && !this->m_var->empty() && m_cursor_pos > 0) {
            if (contains_non_ascii(*this->m_var)) {
                int del_start = prev_char_boundary(*this->m_var, m_cursor_pos);
                this->m_var->erase(del_start, m_cursor_pos - del_start);
                m_char_anims.clear();
                m_char_removing.clear();
                m_ghost_chars.clear();
                m_cursor_pos = del_start;
                return;
            }

            int del_idx = m_cursor_pos - 1;
            if (del_idx < (int)m_char_anims.size()) {
                ghost_char_t ghost{};
                ghost.m_glyph = (*this->m_var)[del_idx];
                ghost.m_anim = m_char_anims[del_idx];
                ghost.m_x = char_x_at_index(*this->m_var, del_idx, (this->m_pos + position).x + 8.f);
                m_ghost_chars.push_back(ghost);
                m_char_anims.erase(m_char_anims.begin() + del_idx);
                m_char_removing.erase(m_char_removing.begin() + del_idx);
            }

            this->m_var->erase(del_idx, 1);
            m_cursor_pos--;
        }

        if (!ImGui::GetIO().InputQueueCharacters.empty()) {
            std::string insert;
            for (ImWchar c : ImGui::GetIO().InputQueueCharacters)
                append_utf8(insert, c);

            if (!insert.empty()) {
                this->m_var->insert(m_cursor_pos, insert);
                m_cursor_pos += (int)insert.size();
            }
        }

    }
}
