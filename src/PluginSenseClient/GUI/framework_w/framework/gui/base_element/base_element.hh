#pragma once

// schizo code
namespace framework
{
	enum class element_type :int
	{
		checkbox,
		slider,
		dropdown,
		button,
		colorpicker,
		keybind,
		text_input,
		listbox,
		interactive_preview,
		popup
	};

	// all elements inherit in this shit
	class c_base_element
	{
	public:
		math::c_vector_2d m_pos{}, m_size{};
		std::string m_label{}, m_parent{};

		// focused element
		// pointer as it needs to be but should it be a shared_pointer?
		c_base_element* m_parent_control{};

		// control visibility, handled by parent(child)
		bool m_visible{}, m_inlined{ false }, m_callback_visibility{};
		std::function<bool()> m_visible_by_callback{ nullptr };

		// focus priority
		focus_priority m_focus_priority{ focus_priority::interactive };
		bool m_search_override{ };

		// parent control width
		float m_parent_width{}; // this should only be accessed from parent and not from current (depends on the need)
		float m_child_size{};

		// control label hiding
		bool m_hide_label{ false };

		std::function<void()> m_call_stacks{};

		// the drawing layer shoud be normal by default but when we are doing popup this needs to be edited
		engine::render_layer m_layer{ engine::render_layer::normal };

		// virtual calls
		virtual void draw() = 0;
		virtual void input() = 0;

		// this will track out the element type, element type that we need to define the element width / etc when its inlined
		element_type m_type{};

		// deconstructor that its kinda useless tbh
		virtual ~c_base_element() = default;
	public: // direct declarations
		void set_parent(std::string parent) {
			this->m_parent = parent;
		}

		// virtual focus_priority m_priority() { return focus_priority::interactive; }

		// element visiblity
		void set_visibility(bool visible)
		{
			this->m_visible = visible;
		}

		// override layering
		void set_layer(engine::render_layer layer)
		{
			this->m_layer = layer;
		}

		void hide_label()
		{
			this->m_hide_label = true;
		}

		void set_callback_visibility(std::function<bool()> fn)
		{
			this->m_callback_visibility = true;
			this->m_visible_by_callback = fn;
		}

		void set_callback_and_inline(std::function<bool()> fn)
		{
			this->m_callback_visibility = true;
			this->m_inlined = true;
			this->m_visible_by_callback = fn;
		}

		void set_inlined()
		{
			this->m_inlined = true;
		}
	};
}