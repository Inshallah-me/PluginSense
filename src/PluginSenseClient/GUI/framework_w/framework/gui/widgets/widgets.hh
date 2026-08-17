#pragma once

namespace framework
{
	class c_widget_ctx
	{
	public:
		math::c_vector_2d m_keybind_pos{10, 50};
		math::c_vector_2d m_notify_panel_pos{10, 10};
	};
	inline c_widget_ctx g_widget_ctx{};

	// widget types
	enum class widget_type : int
	{
		keybind,
		notification_panel
	};

	// base widget class, all widgets inherit from this
	class c_widget_base
	{
	public:
		c_widget_base* focused{};

		// pos and other data
		math::c_vector_2d m_pos{}, m_size{/*base widget size*/};

		math::c_vector_2d prev_mouse_pos{}, delta{}; /* widget moving */
		bool m_widget_dragged{ false };

		// widget type
		widget_type m_type{};

		virtual void draw() = 0;
		virtual void input() = 0;

		utils::anim_context_t m_focus_anim{};
	};

	class c_notify_panel : public c_widget_base
	{
	public:
		struct message_entry {
			std::string text;
			hue::c_color color;
			engine::modifiers::font_flags flags;
			hue::c_color shadow_color;

			struct inline_segment {
				std::string text;
				hue::c_color color;
			};
			std::vector<inline_segment> inlines;

			// lifetime
			float m_lifetime = 0.f;   // elapsed seconds
			float m_max_life = 3.5f;  // total seconds before slide out
			float m_slide_anim = 0.f;   // [0..1] slide in
			bool  m_initialized = false; // first frame snap

			float m_max_width = 0.f;
		};

		c_notify_panel(math::c_vector_2d base_pos);

		void draw() override;
		void input() override;

		c_notify_panel& add(std::string text, hue::c_color color,
			engine::modifiers::font_flags flags = engine::modifiers::font_flags::none,
			hue::c_color shadow_color = engine::modifiers::default_shadow)
		{
			message_entry e{};
			e.text = std::move(text);
			e.color = color;
			e.flags = flags;
			e.shadow_color = shadow_color;
			e.m_lifetime = 0.f;
			e.m_slide_anim = 0.f;
			e.m_initialized = false;
			m_entries.push_back(std::move(e));
			return *this;
		}

		// add a helper that targets the last entry
		c_notify_panel& inlined(std::string text, hue::c_color color)
		{
			if (!m_entries.empty())
				m_entries.back().inlines.push_back({ std::move(text), color });
			return *this;
		}
	private:
		std::vector<message_entry> m_entries;

		utils::anim_context_t m_panel_dash{};
	};

	// keybinds widget
	enum class widget_mode : int
	{
		always,
		hold,
		toggle
	};

	struct keybind_entry_t
	{
		std::string m_label;
		widget_mode m_type;
		std::string m_icon_override;

		keybind_entry_t(std::string label, widget_mode type) : m_label(label), m_type(type) {}
		keybind_entry_t(std::string label, widget_mode type, std::string icon) : m_label(label), m_type(type), m_icon_override(icon) {}
	};

	class c_keybinds : public c_widget_base
	{
	public:
		c_keybinds(math::c_vector_2d base_pos, std::vector<keybind_entry_t> keybinds);

		void draw() override;
		void input() override;

		void update_keybinds(std::vector<keybind_entry_t> new_keybinds) {
			this->keybinds = std::move(new_keybinds);
		}
	private:
		// mugaga
		std::vector<keybind_entry_t> keybinds;

		float get_biggest_size_keybinds(std::vector<keybind_entry_t> values) {
			std::vector<float> sizes;
			for (auto& val : values)
			{
				float widget_x = 0.f;
				if (!val.m_icon_override.empty())
				{
					widget_x = g_font->f_icons.measure(val.m_icon_override.c_str()).x;
				}
				else switch (val.m_type)
				{
					case widget_mode::always:
						widget_x = g_font->f_icons.measure(ICON_FA_CIRCLE_EXCLAMATION).x;
						break;
					case widget_mode::hold:
						widget_x = g_font->f_icons.measure(ICON_FA_HAND_HOLDING).x;
						break;
					case widget_mode::toggle:
						widget_x = g_font->f_icons.measure(ICON_FA_TOGGLE_ON).x;
						break;
				}

				sizes.push_back(
					widget_x + g_font->f_childs.measure(val.m_label.c_str()).x
				);
			}


			std::sort(sizes.begin(), sizes.end(), std::greater<float>());

			return sizes.at(0);
		}
	};

	// main widget manager
	class c_widgets
	{
	public:
		void draw();

		void create_widget(widget_type type, math::c_vector_2d& pos);

		std::shared_ptr<c_keybinds> keybind_manager() {
			return m_manager;
		}

		std::shared_ptr<c_notify_panel> notify() {
			return m_notify_panel;
		}
	private:
		// small vector, nothing big, so iterration wont fuck anything related to performance
		std::vector<std::shared_ptr<c_widget_base>> m_widgets{};

		// single focused vector
		c_widget_base* m_has_focus{}; // one element has been focused

		utils::anim_context_t m_panel_anim{};

		// keybind manager
		std::shared_ptr<c_keybinds> m_manager{};
		std::shared_ptr<c_notify_panel> m_notify_panel{};
	};
}
