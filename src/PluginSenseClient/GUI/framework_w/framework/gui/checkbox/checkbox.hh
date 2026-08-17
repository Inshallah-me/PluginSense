#pragma once

namespace framework
{
	class c_checkbox :public c_base_element
	{
	private:
		bool* m_value{};
	public:
		c_checkbox(std::string label, bool* value);

		void draw() override;
		void input() override;

		c_checkbox* sync_value(std::function<bool()> fn)
		{
			m_call_stacks = [this, fn = std::move(fn)]() {
				*this->m_value = fn();
			};
			return this;
		}

		c_checkbox* on_change(std::function<void(bool)> fn)
		{
			m_on_change = std::move(fn);
			return this;
		}

		c_checkbox* set_enabled(std::function<bool()> fn)
		{
			m_enabled = std::move(fn);
			return this;
		}

	private:
		bool enabled() const { return !m_enabled || m_enabled(); }

		std::function<void(bool)> m_on_change{};
		std::function<bool()> m_enabled{};
	};
}
