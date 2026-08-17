#pragma once

// we do not pass members we do not use shared fuck ya

namespace framework
{
	inline const char* const key_names[] = {
	"none",
	"m1",
	"m2",
	"something",
	"m3",
	"m4",
	"m5",
	"unk",
	"back",
	"tab",
	"unk",
	"unk",
	"clear",
	"enter",
	"unk",
	"unk",
	"shift",
	"control",
	"menu",
	"pause",
	"capital",
	"kana",
	"unk",
	"junja",
	"final",
	"kanja",
	"unk",
	"escape",
	"convert",
	"nonconvert",
	"accept",
	"modechange",
	"space",
	"prior",
	"next",
	"end",
	"home",
	"left",
	"up",
	"right",
	"down",
	"select",
	"print",
	"exec",
	"snap",
	"insert",
	"delete",
	"help",
	"0",
	"1",
	"2",
	"3",
	"4",
	"5",
	"6",
	"7",
	"8",
	"9",
	"unk",
	"unk",
	"unk",
	"unk",
	"unk",
	"unk",
	"unk",
	"a",
	"b",
	"c",
	"d",
	"e",
	"f",
	"g",
	"h",
	"i",
	"j",
	"k",
	"l",
	"m",
	"n",
	"o",
	"p",
	"q",
	"r",
	"s",
	"t",
	"u",
	"v",
	"w",
	"x",
	"y",
	"z",
	"lwin",
	"rwin",
	"apps",
	"unk",
	"sleep",
	"num0",
	"num1",
	"num2",
	"num3",
	"num4",
	"num5",
	"num6",
	"num7",
	"num8",
	"num9",
	"multiply",
	"add",
	"separator",
	"subtract",
	"dec",
	"divide",
	"f1",
	"f2",
	"f3",
	"f4",
	"f5",
	"f6",
	"f7",
	"f8",
	"f9",
	"f10",
	"f11",
	"f12",
	"f13",
	"f14",
	"f15",
	"f16",
	"f17",
	"f18",
	"f19",
	"f20",
	"f21",
	"f22",
	"f23",
	"f24",
	"unk",
	"unk",
	"unk",
	"unk",
	"unk",
	"unk",
	"unk",
	"unk",
	"numlock",
	"scroll",
	"VK_OEM_NEC_EQUAL",
	"VK_OEM_FJ_MASSHOU",
	"VK_OEM_FJ_TOUROKU",
	"VK_OEM_FJ_LOYA",
	"VK_OEM_FJ_ROYA",
	"unk",
	"unk",
	"unk",
	"unk",
	"unk",
	"unk",
	"unk",
	"unk",
	"unk",
	"lshift",
	"rshit",
	"lcontrol",
	"rcontrol",
	"lwin",
	"rwin"
	};

	namespace animations
	{
		inline utils::anim_context_t m_window_opacity{};
		inline utils::anim_context_t m_tab_switching{};

		inline utils::anim_context_t m_checkbox_opacity{};
		inline utils::anim_context_t m_checkbox_value{};
		inline utils::anim_context_t m_checkbox_hover{};
		inline utils::anim_context_t m_checkbox_dezactivated{};

		inline utils::anim_context_t m_slider_opacity{};
		inline utils::anim_context_t m_slider_value{};
		inline utils::anim_context_t m_slider_hover{};
		inline utils::anim_context_t m_slider_dezactivated{};

		inline utils::anim_context_t m_dropdown_opacity{};
		inline utils::anim_context_t m_dropdown_value{};
		inline utils::anim_context_t m_dropdown_open{};
		inline utils::anim_context_t m_dropdown_option_hover{};
		inline utils::anim_context_t m_dropdown_switch{};
		inline utils::anim_context_t m_dropdown_hover{};
		inline utils::anim_context_t m_dropdown_dezactivated{};

		inline utils::anim_context_t m_button_opacity{};
		inline utils::anim_context_t m_button_value{};
		inline utils::anim_context_t m_button_hover{};

		inline utils::anim_context_t m_textinput_opacity{};
		inline utils::anim_context_t m_textinput_value{};
		inline utils::anim_context_t m_textinput_hover{};

		inline utils::anim_context_t m_colorpicker_opacity{};
		inline utils::anim_context_t m_colorpicker_value{};
		inline utils::anim_context_t m_colorpicker_hover{};

		inline utils::anim_context_t m_popup_opacity{};
		inline utils::anim_context_t m_popup_value{};
		inline utils::anim_context_t m_popup_hover{};

		inline utils::anim_context_t m_keybind_opacity{};
		inline utils::anim_context_t m_keybind_value{};
		inline utils::anim_context_t m_keybind_hover{};
		inline utils::anim_context_t m_keybind_selected_mode{};
		inline utils::anim_context_t m_keybind_binding{};

		inline utils::anim_context_t m_listbox_opacity{};
		inline utils::anim_context_t m_listbox_value{};
		inline utils::anim_context_t m_listbox_hover{};
		inline utils::anim_context_t m_listbox_item_hover{};
		inline utils::anim_context_t m_listbox_scroll{};
	}

	// this class will handle the styles, althrough we can push the animations to it tho, doesnt matter that much
	class c_style
	{
	public:
		hue::c_color m_window_background = hue::c_color(17, 17, 17);
		hue::c_color m_element_base = hue::c_color(14, 14, 14);
		hue::c_color m_window_shadow = hue::c_color(0, 0, 0);
		hue::c_color m_window_bars = hue::c_color(25, 25, 25);
		hue::c_color m_text = hue::c_color(255, 255, 255);
		hue::c_color m_accent = hue::c_color(157, 196, 29);
		hue::c_color m_outline = hue::c_color(30, 30, 30);
		hue::c_color m_child_top = hue::c_color(25, 25, 25);
	};
	inline auto g_style = std::make_unique<c_style>();

	// tab data
	struct tab_data_t {
		std::string m_name{}, icon{};
		std::vector<std::string> m_subtab{};

		// other data that gets modified out of box
		int m_active_subtab{};
		std::string m_cur_subtab{};
		bool m_subtabs_present{ false };
	};

	// predefinition
	class c_base_element;

	enum class focus_priority : int {
		none = 0,
		hover,      
		interactive,
		persistent, 
		modal,      
	};

	struct focus_entry_t {
		c_base_element* element;
		focus_priority priority;
	};

	// this class will handle the stuff like menu open vars and other shit
	class c_context
	{
	public:
		bool m_open{ false }, m_dragging{ false }, m_click_consumed{ false };

		std::string m_cur_tab{};
		int m_active_tab{};

		std::vector<tab_data_t> m_tabs{};

		std::string m_username{};

		c_base_element* m_focus_took{};
		c_base_element* m_focus_took_by_popup{};
		c_base_element* m_modal_owner{};
	public:
		std::vector<focus_entry_t> m_focus_stack{};
		c_base_element* m_hovered{};

		void push_focus(c_base_element* element, focus_priority priority) {
			// dont push if already in stack
			for (auto& entry : m_focus_stack)
				if (entry.element == element) return;

			m_focus_stack.push_back({ element, priority });
		}

		void pop_focus(c_base_element* element) {
			m_focus_stack.erase(
				std::remove_if(m_focus_stack.begin(), m_focus_stack.end(),
					[element](const focus_entry_t& e) { return e.element == element; }),
				m_focus_stack.end()
			);
		}

		bool is_focused(c_base_element* element) {
			for (auto& e : m_focus_stack)
				if (e.element == element) return true;
			return false;
		}

		// returns the highest priority holder, nullptr if none
		c_base_element* top_focus() {
			if (m_focus_stack.empty()) return nullptr;
			return std::max_element(m_focus_stack.begin(), m_focus_stack.end(),
				[](const focus_entry_t& a, const focus_entry_t& b) {
					return (int)a.priority < (int)b.priority;
				})->element;
		}

		bool can_interact(c_base_element* element, focus_priority my_priority) {
			// if we're inside a modal context, all elements are free to interact
			if (m_modal_owner != nullptr)
				return true;

			auto top = top_focus();
			if (top == nullptr) return true;
			if (top == element) return true;
			auto top_priority = std::max_element(m_focus_stack.begin(), m_focus_stack.end(),
				[](const focus_entry_t& a, const focus_entry_t& b) {
					return (int)a.priority < (int)b.priority;
				})->priority;
			return (int)my_priority > (int)top_priority;
		}
	public:
		std::string get_key_name(const int VirtualKey) {
			auto Code = MapVirtualKeyA(VirtualKey, MAPVK_VK_TO_VSC);

			int Result;
			char Buffer[128];

			switch (VirtualKey) {
			case VK_LEFT: case VK_UP: case VK_RIGHT: case VK_DOWN:
			case VK_RCONTROL: case VK_RMENU:
			case VK_LWIN: case VK_RWIN: case VK_APPS:
			case VK_PRIOR: case VK_NEXT:
			case VK_END: case VK_HOME:
			case VK_INSERT: case VK_DELETE:
			case VK_DIVIDE:
			case VK_NUMLOCK:
				Code |= KF_EXTENDED;
			default:
				Result = GetKeyNameTextA(Code << 16, Buffer, 128);
			}

			if (Result == 0 || VirtualKey == VK_CAPITAL) {
				switch (VirtualKey) {
				case VK_XBUTTON1:
					return ("M4");
				case VK_XBUTTON2:
					return ("M5");
				case VK_LBUTTON:
					return ("M1");
				case VK_MBUTTON:
					return ("M3");
				case VK_RBUTTON:
					return ("M2");
				case VK_CAPITAL:
					return ("CAPS");
				default:
					return ("Bind a key!");
				}
			}

			auto transformer = std::string(Buffer);
			std::transform(transformer.begin(), transformer.end(), transformer.begin(), ::toupper);

			return transformer;
		}
	};
	inline auto g_ctx = std::make_unique<c_context>();
}
