#pragma once

namespace framework
{
	enum key_mode_t : int {
		// this is a bit ghetto but we can use this to define the keybind mode
		always = 0,
		hold = 1,
		toggle = 2,
	};

	// TOTAL DOSHIT
	struct key_var_t {
		int key{}, mode{ 1 };
		int runtime_key{};
		int runtime_mode{ 1 };
		bool runtime_toggled{};
		bool runtime_was_down{};
		bool runtime_ignore_until_released{};

		void reset_runtime_state() {
			runtime_key = key;
			runtime_mode = mode;
			runtime_toggled = false;
			runtime_was_down = false;
			runtime_ignore_until_released = false;
		}

		void suppress_until_released() {
			runtime_key = key;
			runtime_mode = mode;
			runtime_toggled = false;
			runtime_was_down = false;
			runtime_ignore_until_released = key > 0 && key <= 255 && (GetAsyncKeyState(key) & 0x8000) != 0;
		}

		// yeah i guess we can make this our main input system for keybind
		bool active(bool bound_to_box = false) {
			if ((this->key <= 0 || this->key > 255) && this->mode != key_mode_t::always) {
				// the key is not set so what the fucking ever
				reset_runtime_state();
				return false; // invalid key
			}

			if (this->mode == key_mode_t::always) {
				// this is always on
				reset_runtime_state();
				return true;
			}

			const bool key_changed = runtime_key != key || runtime_mode != mode;
			const bool down = (GetAsyncKeyState(key) & 0x8000) != 0;
			if (key_changed) {
				runtime_key = key;
				runtime_mode = mode;
				runtime_toggled = false;
				runtime_was_down = down;
				runtime_ignore_until_released = down;
				return false;
			}

			if (runtime_ignore_until_released) {
				runtime_was_down = down;
				if (!down)
					runtime_ignore_until_released = false;
				return false;
			}

			if (this->mode == key_mode_t::toggle) {
				if (down && !runtime_was_down)
					runtime_toggled = !runtime_toggled;

				runtime_was_down = down;
				return runtime_toggled;
			}
			else if (this->mode == key_mode_t::hold) {
				runtime_was_down = down;
				return down; // this should return true or false
			}
			else {
				reset_runtime_state();
				return false;
			}
		}
	};

	class c_keybind : public c_base_element
	{
	public:
		c_keybind(std::string label, key_var_t* val, bool hide_label = false);

		void draw() override;
		void input() override;

		c_keybind* key_only()
		{
			m_key_only = true;
			return this;
		}

		c_keybind* keyboard_only()
		{
			m_allow_mouse = false;
			return this;
		}

		c_keybind* suppress_next_keyup()
		{
			m_suppress_next_keyup = true;
			return this;
		}

		c_keybind* set_enabled(std::function<bool()> fn)
		{
			m_enabled = std::move(fn);
			return this;
		}
	private:
		key_var_t* m_val{};
		bool m_key_only{};
		bool m_allow_mouse{ true };
		bool m_suppress_next_keyup{ false };
		std::function<bool()> m_enabled{};

		bool enabled() const { return !m_enabled || m_enabled(); }

		// this var exists only for keybind objects, no fucking way we make it a global, its just useless
		bool m_key_callback{false}; // set it to false by default
		bool m_waiting_for_release{ false };
		std::array<bool, 256> m_key_down{};
	};
}
