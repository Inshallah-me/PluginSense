#pragma once

namespace framework
{
	struct child_data_t {
		std::string m_tab_name{};
		std::string m_subtab_name{};

		int tab{};
	};

	enum class child_width : int {
		full,
		half
	};

	// predefinition
	class c_window;

	class c_child : public std::enable_shared_from_this<c_child>
	{
		friend class c_window;
	public:
		c_child(std::string name, child_width width, float y);

		void draw();
		void input();

		void attach_child(std::string tab_name, std::string subtab_name, int tab_index);

		void set_visible(bool data);
		bool visible();

		child_width get_type();

		math::c_vector_2d calculate_element_padding();
		math::c_vector_2d calculate_safe_area();

		void stack_calls(std::function<void()> fn)
		{
			this->fn = fn;
		}

		// elements
		std::shared_ptr<c_checkbox> add_checkbox(std::string label, bool* val);
		std::shared_ptr<c_slider_float> add_slider_float(std::string label, float* val, float min, float max, bool hide_label = false, std::wstring prefix = L"", int precision = 1);
		std::shared_ptr<c_slider_int> add_slider_int(std::string label, int* val, int min, int max, bool hide_label = false, std::wstring prefix = L"");
		std::shared_ptr<c_dropdown> add_dropdown(std::string label, int* val, std::vector<std::string> items, bool hide_label = false);
		std::shared_ptr<c_multidropdown> add_multibox(std::string label, bool hide_label, std::function<void(c_multidropdown* ptr)> callback);
		std::shared_ptr<c_button> add_button(std::string label, std::function<void()> callback);
		std::shared_ptr<c_colorpicker> add_colorpicker(std::string label, hue::c_color* val, bool hide_label = false);
		std::shared_ptr<c_keybind> add_keybind(std::string label, key_var_t* val, bool hide_label = false);
		std::shared_ptr<c_text_input> add_input_box(std::string label, std::string* val, bool hide_label = false);
		std::shared_ptr<c_listbox> add_listbox(std::string label, int* val, std::vector<std::string> items, float height, bool hide_label = false);
		std::shared_ptr<c_popup> add_popup(std::string label, bool hide_label, std::function<void(c_popup* ptr)> callback);
		std::shared_ptr<c_interactive_preview> add_interactive_preview();

		// animation data
		utils::anim_context_t m_child_opacity{};
		utils::anim_context_t m_child_slide{};
	private:
		std::function<void()> fn{};

		std::string m_name{};

		math::c_vector_2d m_pos{}, m_relative_pos{};
		math::c_vector_2d m_size{};
		child_width m_width_type{};

		// data about where the child should be attached
		child_data_t m_child_attach_data{};

		// this requests a comment
		// so basically this will determine whether we draw the child or not
		bool m_visible{ false };

		// check if this child has titlebar
		// by default its true, can be overrided from gui.cpp/child creation, ptr->remove_titlebar();
		bool m_titlebar{ true };

		// elements inherited by this child
		std::vector<std::shared_ptr<c_base_element>> m_controls{};

		// scrolling data
		float m_scroll_offset = 0.f;
		float m_scroll_target = 0.f;
		float m_max_scroll = 0.f;
		float m_content_height = 0.f;
	};

}