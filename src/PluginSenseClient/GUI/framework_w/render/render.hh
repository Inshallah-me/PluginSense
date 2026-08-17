#pragma once

namespace engine
{
	enum draw_flags_
	{
		draw_flags_none = 0,
		draw_flags_closed = 1 << 0,
		draw_flags_round_corners_top_left = 1 << 4,
		draw_flags_round_corners_top_right = 1 << 5,
		draw_flags_round_corners_bottom_left = 1 << 6,
		draw_flags_round_corners_bottom_right = 1 << 7,
		draw_flags_round_corners_none = 1 << 8,
		draw_flags_round_corners_top = draw_flags_round_corners_top_left | draw_flags_round_corners_top_right,
		draw_flags_round_corners_bottom = draw_flags_round_corners_bottom_left | draw_flags_round_corners_bottom_right,
		draw_flags_round_corners_left = draw_flags_round_corners_bottom_left | draw_flags_round_corners_top_left,
		draw_flags_round_corners_right = draw_flags_round_corners_bottom_right | draw_flags_round_corners_top_right,
		draw_flags_round_corners_all = draw_flags_round_corners_top_left | draw_flags_round_corners_top_right | draw_flags_round_corners_bottom_left | draw_flags_round_corners_bottom_right,
		draw_flags_round_corners_default = draw_flags_round_corners_all,
		draw_flags_round_corners_mask = draw_flags_round_corners_all | draw_flags_round_corners_none,
		draw_flags_shadow_cut_out_shape_background = 1 << 9
	};
	typedef int draw_flags;

	enum fade_direction : int
	{
		vertically,
		horizontally,
		diagonally,
		diagonally_reversed,
	};

	enum class render_layer : int {
		normal = 0,
		middle,
		prioritized,
		modal,
		count
	};

	class c_gradient_data {
	public:
		hue::c_color m_top_r{}, m_top_l{};
		hue::c_color m_bot_r{}, m_bot_l{};
	public:
		c_gradient_data(hue::c_color top_r, hue::c_color top_l, hue::c_color bottom_r, hue::c_color bottom_l) : m_top_r(top_r), m_top_l(top_l),
			m_bot_r(bottom_r), m_bot_l(bottom_l) {
		}
	};

	// macros
	using vector_t = ImVec2;
	using draw_list_t = ImDrawList;

	struct rotation_data_t {
		void set_draw_list(draw_list_t* draw_list)
		{
			this->m_draw_list = draw_list;
		}

		void rotation_start()
		{
			this->m_start_index = this->m_draw_list->VtxBuffer.Size;
		}

		vector_t rotation_center()
		{
			vector_t l(FLT_MAX, FLT_MAX), u(-FLT_MAX, -FLT_MAX);

			const auto& buf = this->m_draw_list->VtxBuffer;
			for (int i = this->m_start_index; i < buf.Size; i++)
				l = ImMin(l, buf[i].pos), u = ImMax(u, buf[i].pos);

			return vector_t((l.x + u.x) / 2, (l.y + u.y) / 2);
		}

		void rotation_end(float rad, vector_t center)
		{
			float s = sin(rad), c = cos(rad);

			const auto r = ImRotate(center, s, c);

			center = vector_t(r.x - center.x, r.y - center.y);

			auto& buf = this->m_draw_list->VtxBuffer;
			for (int i = this->m_start_index; i < buf.Size; i++) {
				const auto j = ImRotate(buf[i].pos, s, c);

				buf[i].pos = vector_t(j.x - center.x, j.y - center.y);
			}
		}

		int get_start_index()
		{
			return this->m_start_index;
		}

	private:
		int m_start_index = { };
		draw_list_t* m_draw_list = { };
	};

	namespace modifiers {
		enum font_flags {
			none,
			drop_shadow,
			outline, 
			blur_shadow
		};

		// default stuff
		inline hue::c_color default_shadow = hue::c_color(0, 0, 0, 200);
	}
}

namespace input {
	enum mouse_buttons {
		left = 0,
		right = 1,
		middle = 2,
		count = 5
	};

	/* @c_input_controller:
		class that will handle all the input system for the gui and overall the input

		details:
			- the whole input is handled by imgui
	*/
	class c_input_controller {
	public:
		/* @get_mouse_position()
			function that will return the current mouse position
		*/
		math::c_vector_2d get_mouse_position();

		/* @mouse_in_region()
			function that will check if the mouse is in a specific region
		*/
		bool mouse_in_region(const math::c_vector_2d& pos, const math::c_vector_2d& size);

		/* @clicked:
			returns the state of mouse if has been clicked on the current frame
		*/
		bool clicked(mouse_buttons button);

		/* @click_down:
			returns the state of mouse if is being held down
		*/
		bool click_down(mouse_buttons button);

		/* @click_released:
			returns the state of mouse if has been released on the current frame
		*/
		bool click_released(mouse_buttons button);

		/* @get_wheel_value:
			returns the cur wheel pos val
		*/
		float get_wheel_value();
	};
}
inline std::unique_ptr<input::c_input_controller> g_input = std::make_unique<input::c_input_controller>();

class c_font {
public:
	class text_builder {
	public:
		struct segment {
			std::string text;
			hue::c_color color;
		};

		text_builder(c_font* font, float x, float y,
			std::string text, hue::c_color color,
			engine::modifiers::font_flags flags,
			hue::c_color shadow_color)
			: m_font(font), m_x(x), m_y(y),
			m_flags(flags), m_shadow_color(shadow_color)
		{
			m_segments.push_back({ std::move(text), color });
		}

		text_builder& inlined(std::string text, hue::c_color color = {}) {
			m_segments.push_back({ std::move(text), color });
			return *this;
		}

		~text_builder() {
			if (!m_font) return;
			ImGui::PushFont(m_font->m_handle);

			float cursor_x = m_x;
			for (auto& seg : m_segments) {
				m_font->draw_segment(cursor_x, m_y, seg.text, seg.color, m_flags, m_shadow_color);
				cursor_x += m_font->measure(seg.text).x + 1;
			}

			ImGui::PopFont();
		}

		text_builder(const text_builder&) = delete;
		text_builder& operator=(const text_builder&) = delete;
		text_builder(text_builder&& other) noexcept
			: m_font(other.m_font), m_x(other.m_x), m_y(other.m_y),
			m_flags(other.m_flags), m_shadow_color(other.m_shadow_color),
			m_segments(std::move(other.m_segments))
		{
			other.m_font = nullptr; // disarm the moved-from destructor
		}

	private:
		c_font* m_font;
		float                           m_x, m_y;
		engine::modifiers::font_flags   m_flags;
		hue::c_color                    m_shadow_color;
		std::vector<segment>            m_segments;
	};

	bool create(const std::string& font_location, float size);
	bool create(const std::string& font_location, float size, const ImFontConfig* font_template,
		const ImWchar* glyph);
	bool create(void* data, int sizess /*, sizeof(data) */, float size, std::string id, bool compresed = false);
	bool create(void* data, int font_size /*, sizeof(data) */, float size, const ImFontConfig* font_template,
		const ImWchar* glyph, std::string id, bool compresed = false);
public:
	math::c_vector_2d measure(const std::string& text);

	void string(float x, float y, std::string text, hue::c_color color);
	//void text(int x, int y, std::string text, hue::c_color color, engine::modifiers::font_flags flags = engine::modifiers::font_flags::none, hue::c_color shadow_color = engine::modifiers::default_shadow,
	//	bool double_layered = false, std::string second_text = "", hue::c_color second_color = hue::c_color());

	// returns builder — destructor fires render automatically
	text_builder text(float x, float y, std::string text, hue::c_color color,
		engine::modifiers::font_flags flags = engine::modifiers::font_flags::none,
		hue::c_color shadow_color = engine::modifiers::default_shadow)
	{
		return text_builder(this, x, y, std::move(text), color, flags, shadow_color);
	}

	ImFont* get_font() {
		return this->m_handle;
	}
private:
	void draw_segment(float x, float y, const std::string& text, hue::c_color color,
		engine::modifiers::font_flags flags, hue::c_color shadow_color)
	{
		if (flags == engine::modifiers::drop_shadow) {
			this->string(x + 1, y + 1, text, shadow_color);
		}
		else if (flags == engine::modifiers::outline) {
			this->string(x + 1, y - 1, text, shadow_color);
			this->string(x - 1, y + 1, text, shadow_color);
			this->string(x - 1, y - 1, text, shadow_color);
			this->string(x + 1, y + 1, text, shadow_color);
			this->string(x, y + 1, text, shadow_color);
			this->string(x, y - 1, text, shadow_color);
			this->string(x + 1, y, text, shadow_color);
			this->string(x - 1, y, text, shadow_color);
		}
		else if (flags == engine::modifiers::blur_shadow) {
			const float base = shadow_color.a / 255.f;

			static const struct { int x, y; float a; } kernel[] = {
				{ 1, 0, 0.50f}, {-1, 0, 0.50f}, { 0, 1, 0.50f}, { 0,-1, 0.50f},
				{ 1, 1, 0.35f}, {-1, 1, 0.35f}, { 1,-1, 0.35f}, {-1,-1, 0.35f},
				{ 2, 0, 0.25f}, {-2, 0, 0.25f}, { 0, 2, 0.25f}, { 0,-2, 0.25f},
				{ 2, 1, 0.20f}, {-2, 1, 0.20f}, { 2,-1, 0.20f}, {-2,-1, 0.20f},
				{ 1, 2, 0.20f}, {-1, 2, 0.20f}, { 1,-2, 0.20f}, {-1,-2, 0.20f},
				{ 2, 2, 0.14f}, {-2, 2, 0.14f}, { 2,-2, 0.14f}, {-2,-2, 0.14f},
				{ 3, 0, 0.10f}, {-3, 0, 0.10f}, { 0, 3, 0.10f}, { 0,-3, 0.10f},
				{ 3, 1, 0.08f}, {-3, 1, 0.08f}, { 3,-1, 0.08f}, {-3,-1, 0.08f},
				{ 1, 3, 0.08f}, {-1, 3, 0.08f}, { 1,-3, 0.08f}, {-1,-3, 0.08f},
				{ 3, 2, 0.06f}, {-3, 2, 0.06f}, { 3,-2, 0.06f}, {-3,-2, 0.06f},
				{ 2, 3, 0.06f}, {-2, 3, 0.06f}, { 2,-3, 0.06f}, {-2,-3, 0.06f},
				{ 3, 3, 0.04f}, {-3, 3, 0.04f}, { 3,-3, 0.04f}, {-3,-3, 0.04f},
				{ 4, 0, 0.04f}, {-4, 0, 0.04f}, { 0, 4, 0.04f}, { 0,-4, 0.04f},
				{ 4, 1, 0.03f}, {-4, 1, 0.03f}, { 4,-1, 0.03f}, {-4,-1, 0.03f},
				{ 1, 4, 0.03f}, {-1, 4, 0.03f}, { 1,-4, 0.03f}, {-1,-4, 0.03f},
				{ 4, 2, 0.02f}, {-4, 2, 0.02f}, { 4,-2, 0.02f}, {-4,-2, 0.02f},
				{ 2, 4, 0.02f}, {-2, 4, 0.02f}, { 2,-4, 0.02f}, {-2,-4, 0.02f},
			};

			for (auto& k : kernel) {
				hue::c_color sc = shadow_color;
				sc.a = (uint8_t)(255.f * base * k.a);

				if (sc.a < 2)
					continue;

				this->string(x + k.x, y + k.y, text, sc);
			}
		}

		this->string(x, y, text, color);
	}

	ImFont* m_handle{};
	math::c_vector_2d m_size{};
};

class c_texture
{
public:
	// c_texture() = default;
	void load_from_file(const char* filename);
	void load_from_memory(unsigned char* Memory, UINT size);

	ID3D11ShaderResourceView* m_srv{};

	int m_width{};
	int m_height{};
private:


	bool m_loaded{ false };
};

struct image_t
{
	c_texture preview{};
	c_texture m_leaf_texture{};
};
inline image_t g_texture{};

struct fonts_t {
	c_font f_default{};
	c_font f_childs{};
	c_font f_bold{};
	c_font f_icons{};
	c_font f_icons_rs{};
	c_font f_icons_medium{};
	c_font f_icons_lucide{};
	c_font f_weapon_icons{}; // esp_icons 武器图标(独立字体,ASCII 码点与主字体冲突,不能 merge)

	c_font f_verdana{};
	c_font f_smallest_pixel{};
	c_font f_oof_arrow{};

	inline void build_fonts(ImFontAtlas* atlas) {
		ImGuiFreeType::BuildFontAtlas(atlas, 0); // ImGuiFreeTypeBuilderFlags_ForceAutoHint
	}
};
inline std::shared_ptr<fonts_t> g_font = std::make_shared<fonts_t>();

class c_render {
private:
	ImDrawList* m_draw_list{};

	ID3D11DeviceContext* context{ nullptr };
public: /* inside declaractions */
	void set_draw_list(ImDrawList* draw_list) {
		this->m_draw_list = draw_list;
	}

	ImDrawList* draw_list() {
		return this->m_draw_list;
	}

	void set_ctx(ID3D11DeviceContext* ctx) {
		this->context = ctx;
	}

	ID3D11DeviceContext* get_ctx() {
		return this->context;
	}

	ID3D11ShaderResourceView* override_resource{ nullptr };
	void override_texture(ID3D11ShaderResourceView* resource) {
		this->override_resource = resource;
	}
public: /* layering system */
	void begin_layers();
	void set_layer(engine::render_layer layer);
	void end_layers();

	template <typename function>
	void use_layer(engine::render_layer layer, function&& func, bool ignore_clipping = false) {
		ImDrawList* previous_draw_list = this->m_draw_list; // save current

		this->set_layer(layer);

		if (ignore_clipping)
			this->m_draw_list = ImGui::GetForegroundDrawList();

		func();

		if (ignore_clipping)
			this->m_draw_list = previous_draw_list; // restore to what it was, not blindly to background

		if (layer != engine::render_layer::normal) {
			this->set_layer(engine::render_layer::normal);
		}
	}
public: /* drawing engine */
	void rect_shadow(float x, float y, float w, float h, hue::c_color col, float thickness = 25.f, float rouding = 0.f);
	void rect_filled(float x, float y, float w, float h, hue::c_color col, float rounding = 0.f, engine::draw_flags flags = engine::draw_flags_none);
	void rect(float x, float y, float w, float h, hue::c_color col, float rounding = 0.f, float thickness = 0.5f);
	void image(float x, float y, float w, float h, ImTextureID texture_id, hue::c_color col, float rounding = 0.f);

	void set_linear_alpha(int vert_start_idx, int vert_end_idx, ImVec2 gradient_p0, ImVec2 gradient_p1, ImU32 col0, ImU32 col1);
	void fade_rect(float x, float y, float w, float h, hue::c_color col, hue::c_color col2, engine::fade_direction direction, float rounding, float thickness);
	void fade_rect_filled(float x, float y, float w, float h, hue::c_color col1, hue::c_color col2, engine::fade_direction direction, float rounding = 0.f, engine::draw_flags flags = engine::draw_flags_none);

	void enlarged_arrow(math::c_vector_2d pos, hue::c_color col, int dir, float scale);

	void radial_gradient_rect_filled(float x, float y, float w, float h, ImVec2 size, float rounding, hue::c_color col1, hue::c_color col2);

	void gradient(math::c_vector_2d pos, math::c_vector_2d size, hue::c_color color, hue::c_color color2, engine::fade_direction flags,
		float rounding = 0.f, hue::c_color backround_helper = hue::c_color(), ImDrawFlags draw_flags = ImDrawFlags_RoundCornersAll);
	void gradient(float x, float y, float w, float h, hue::c_color color, hue::c_color color2, engine::fade_direction flags,
		float rounding = 0.f, hue::c_color backround_helper = hue::c_color(), ImDrawFlags draw_flags = 0);

	void dashed_rect(float x, float y, float w, float h, hue::c_color col, float rounding, float thickness, float dash_length, float gap_length);

	void add_shadow_line(math::c_vector_2d p1, math::c_vector_2d p2, hue::c_color col, float thickness)
	{
		ImU32 imgui_col = col.transform();
		if ((imgui_col & IM_COL32_A_MASK) == 0)
			return;

		m_draw_list->PathLineTo(p1.transform());
		m_draw_list->PathLineTo({ p1.x + 0.5f, p1.y + 0.5f });

		m_draw_list->PathLineTo({ p2.x + 0.5f, p2.y + 0.5f });
		m_draw_list->PathLineTo(p2.transform());

		m_draw_list->AddShadowConvexPoly(m_draw_list->_Path.Data, m_draw_list->_Path.Size, imgui_col, thickness, math::c_vector_2d(0.f, 0.f).transform(), 0);
		m_draw_list->_Path.Size = 0;
	}

	void radial_gradient(float x, float y, float w, float h, float radius, hue::c_color col1, hue::c_color col2);
	void rect_filled_multi_color(float x, float y, float w, float h, engine::c_gradient_data gradient_data, float rounding = 0.f, engine::draw_flags flags = engine::draw_flags_none);

	void check_mark(float x, float y, float size, hue::c_color col);

	void push_clip(float x, float y, float w, float h);
	void restore_clip();
public: /* main */
	void setup();
};

inline std::unique_ptr<c_render> g_render = std::make_unique<c_render>();

/* font includes */
#include "fonts/fa.hh"
#include "fonts/fa6.hh"
#include "fonts/sf_pro.hh"
#include "fonts/sf_pro_bold.hh"
#include "fonts/bg.hh"
