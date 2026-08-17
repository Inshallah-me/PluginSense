#pragma once

namespace framework
{
    class c_text_input : public c_base_element
    {
    public:
        c_text_input(std::string label, std::string* var, bool hide_label = false);
        void draw() override;
        void input() override;

    private:
        std::string* m_var{};
        struct ghost_char_t {
            char  m_glyph;
            float m_anim;
            float m_x;
        };

        std::vector<ghost_char_t> m_ghost_chars;

        std::vector<float> m_char_anims;
        std::vector<bool>  m_char_removing;

        float blink{};
        int m_cursor_pos = 0;
        float m_scroll_offset = 0.f;
    };
}
