#pragma once

namespace framework
{
	class c_popup : public c_base_element
	{
	public:
		c_popup(std::string label, bool hide_label = false);

		void draw() override;
		void input() override;

		bool m_headless{ false };
		bool m_hide_popup_title{ false };

		void hide_popup_title()
		{
			m_hide_popup_title = true;
		}

		std::shared_ptr<c_checkbox> add_checkbox(std::string label, bool* val);
		std::shared_ptr<c_slider_float> add_slider_float(std::string label, float* val, float min, float max, bool hide_label = false, std::wstring prefix = L"");
		std::shared_ptr<c_slider_int> add_slider_int(std::string label, int* val, int min, int max, bool hide_label = false, std::wstring prefix = L"");
		std::shared_ptr<c_dropdown> add_dropdown(std::string label, int* val, std::vector<std::string> items, bool hide_label = false);
		std::shared_ptr<c_multidropdown> add_multibox(std::string label, bool hide_label, std::function<void(c_multidropdown* ptr)> callback);
		std::shared_ptr<c_button> add_button(std::string label, std::function<void()> callback);
		std::shared_ptr<c_colorpicker> add_colorpicker(std::string label, hue::c_color* val, bool hide_label = false);
		std::shared_ptr<c_keybind> add_keybind(std::string label, key_var_t* val, bool hide_label = false);
		std::shared_ptr<c_text_input> add_input_box(std::string label, std::string* val, bool hide_label = false);
		
		// std::shared_ptr<c_popup> add_popup(std::string label, bool hide_label, std::function<void(c_popup* ptr)> callback);
	private:
		std::vector<std::shared_ptr<c_base_element>> m_controls{};

		// this will get increased once we add elements
		float m_height{ 30.f };
	};
}
