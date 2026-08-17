#pragma once

namespace framework
{
	enum class object_types : int
	{
		box,
		text,
		bars
	};

	// base object class for box, name, etc.
	class c_grid_system;
	class c_base_object
	{
	public:
		std::string m_label{};

		math::c_vector_2d m_pos{}, m_target_pos{};
		math::c_vector_2d m_size{};

		bool* m_visible_flag{ nullptr };

		// for bars
		hue::c_color m_def_color{};

		object_types m_type{};
		c_grid_system* m_drop_state{ nullptr };

		c_font m_font{g_font->f_verdana};
		int m_custom_font{};

		// optional context popup, opened on right-click on this object
		std::shared_ptr<c_popup> m_popup{};

		// virtual calls
		virtual void draw() = 0;
		virtual void input() = 0;

		utils::anim_context_t m_hover_animation{};

		// bbox that is pushed from ip
		math::c_rect m_bbox{};

		void callback(bool* var)
		{
			m_visible_flag = var;
		}

		// chainable popup attach -- mirrors c_child::add_popup style
		void attach_popup(std::function<void(c_popup* ptr)> builder)
		{
			m_popup = std::make_shared<c_popup>(m_label + "_ctx", true);
			m_popup->m_headless = true;
			builder(m_popup.get());
		}

		void default_font(c_font font)
		{
			this->m_font = font;
		}

		void update_font(int font_setting)
		{
			this->m_custom_font = font_setting;

			switch (font_setting)
			{
				case 0:
				{
					this->m_font = g_font->f_verdana;
				} break;
				case 1:
				{
					this->m_font = g_font->f_smallest_pixel;
				} break;
			}
		}
	private:
		bool object_in_area(const math::c_vector_2d& pos, const math::c_vector_2d& size)
		{
			return (this->m_pos.x > pos.x && this->m_pos.y > pos.y &&
				this->m_pos.x < pos.x + size.x && this->m_pos.y < pos.y + size.y);
		}
	};

	class c_interactive_globals
	{
	public:
		c_base_object* m_hovered_object{};
		c_base_object* m_focused_object{};
		int m_insert_index{ 0 };
		math::c_vector_2d m_drag_offset{ };

		bool m_dragging_object{ false };
	};
	inline c_interactive_globals g_ip_globals;

	// bounding box object
	class c_bounding_box :public c_base_object
	{
	public:
		// no need to push this to c file
		c_bounding_box()
		{
			m_type = object_types::box;
		}

		void draw() override;
		void input() override;
	};

	class c_text_object : public c_base_object
	{
	public:
		c_text_object(std::string label)
		{
			m_label = std::move(label);
			m_type = object_types::text;

			// we need to pass size manually
			m_size = { m_font.measure(m_label).x, m_font.measure(m_label).y };
		}

		void draw() override;
		void input() override;
	};

	class c_bar_object : public c_base_object
	{
	public:
		c_bar_object(std::string label, hue::c_color default_color)
		{
			m_label = std::move(label);
			m_def_color = std::move(default_color);
			m_type = object_types::bars;
		}

		void draw() override;
		void input() override;
	};

	// grid system
	enum class grid_areas : int
	{
		normal,
		top,
		bottom,
		left,
		right
	};

	class c_grid_system
	{
	public:
		c_grid_system(grid_areas area);

		void draw();

		// for input update on the grid
		void input(math::c_rect m_bbox);

		// esp elements
		std::vector<std::shared_ptr<c_base_object>> m_objects{};
	public:
		bool object_in_area(const math::c_vector_2d& pos, const math::c_vector_2d& size)
		{
			// AABB overlap test between the supplied rect and this grid's rect
			return !(pos.x + size.x < m_pos.x || pos.x > m_pos.x + m_size.x ||
				pos.y + size.y < m_pos.y || pos.y > m_pos.y + m_size.y);
		}

		grid_areas m_area{};
		math::c_vector_2d m_pos{}, m_relative_pos{}, m_size{};

		float m_cursor{ 0.f };
		float m_wcursor{ 0.f };
	};

	struct laid_out_element_t
	{
		c_base_object* m_object{ nullptr };
		std::string m_label{};
		object_types m_type{};
		math::c_vector_2d m_pos{};
		math::c_vector_2d m_size{};
	};

	// this will draw outside of the main menu so it wont be inited in any circumstances from the child
	class c_interactive_preview :public c_base_element
	{
	public:
		// y size will be menu y
		c_interactive_preview();

		void draw() override;
		void input() override;

		std::shared_ptr<c_bounding_box> add_box();
		std::shared_ptr<c_text_object> add_text_object(std::string label, grid_areas area);
		std::shared_ptr<c_bar_object> add_bar_object(std::string label, hue::c_color default_color, grid_areas area);

		std::vector<laid_out_element_t> extract_layout(
			math::c_rect& target_box,
			std::function<math::c_vector_2d(c_base_object*)> size_for_text = nullptr) const;

		laid_out_element_t resolve(
			const std::string& label,
			math::c_rect& target_box,
			std::function<math::c_vector_2d(c_base_object*)> size_for_text = nullptr) const;
	private:
		void update_bbox();

		// esp elements
		std::vector<std::shared_ptr<c_grid_system>> m_grid{};

		math::c_rect m_bbox{};
	};
}