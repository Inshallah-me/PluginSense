#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace framework
{
	struct multidropdown_data_t {
		std::string m_label{};
		bool* m_val{};
	};

	class c_multidropdown : public c_base_element, public std::enable_shared_from_this<c_multidropdown> {
	public:
		c_multidropdown(std::string label, bool hide_label = false);

		void draw() override;
		void input() override;

		void add_selection(std::string label, bool* val);

		// 每项一个图标字符(空 = 无图标),用独立图标字体渲染在文字前
		std::shared_ptr<c_multidropdown> icon_stack(std::function<std::vector<std::string>()> fn)
		{
			m_icon_stacks = std::move(fn);
			return shared_from_this();
		}
	private:
		std::vector< multidropdown_data_t> m_data{};
		std::function<std::vector<std::string>()> m_icon_stacks{};
		float m_scroll_offset{};
		float m_scroll_target{};
	};
}
