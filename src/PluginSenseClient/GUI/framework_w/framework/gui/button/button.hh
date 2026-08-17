#pragma once

namespace framework
{
	struct ripple_t {
		float m_anim;
	};

	class c_button : public c_base_element
	{
	public:
		c_button(std::string label, std::function<void()> callback);

		void draw() override;
		void input() override;

		c_button* execute_stack(std::function<std::string()> fn)
		{
			m_call_stacks = [this, fn = std::move(fn)]() {
				this->m_label = fn();
			};

			return this;
		}
	private:
		std::function<void()> m_callback;

		bool m_callback_called{ false };

		std::vector<ripple_t> m_ripples;
	};
}
