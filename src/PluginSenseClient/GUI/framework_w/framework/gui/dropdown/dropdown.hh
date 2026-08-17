#pragma once

namespace framework
{
	class c_dropdown : public c_base_element, public std::enable_shared_from_this<c_dropdown> {
	public:
		c_dropdown(std::string label, int* val, std::vector<std::string> items, bool hide_label = false);

		void draw() override;
		void input() override;

		std::shared_ptr<c_dropdown> execute_stack(std::function<std::vector<std::string>()> fn)
		{
			m_call_stacks = [this, fn = std::move(fn)]() {
				this->m_items = fn();
				};

			return shared_from_this();
		}

		// 每项一个图标字符(空 = 无图标),用独立图标字体渲染在文字前
		std::shared_ptr<c_dropdown> icon_stack(std::function<std::vector<std::string>()> fn)
		{
			m_icon_stacks = std::move(fn);
			return shared_from_this();
		}
	private:
		int* m_val{};
		std::vector<std::string> m_items{};
		std::function<std::vector<std::string>()> m_icon_stacks{};
		float m_scroll_offset{};
		float m_scroll_target{};
	};
}