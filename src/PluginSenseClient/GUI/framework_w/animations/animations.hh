#pragma once

#pragma warning (disable : 4996)

namespace utils
{
	inline std::map<unsigned int, float> stack;
	inline std::map<unsigned int, math::c_vector_2d> vector_stack;

	struct anim_context_t {
		float m_value;
		unsigned int m_id;

		float m_original_value;
		float m_limited_min = 0.f;
		float m_limited_max = 1.f;
		bool m_is_limited = false;

		inline void animate(float adjust_to, float min = 0.f, float max = 1.f, bool clamp = true) {
			auto animation = stack.find(m_id);
			if (animation == stack.end()) {
				stack.insert({ m_id, 0.f });

				animation = stack.find(m_id);
			}

			clamp ? animation->second = std::clamp(adjust_to, min, max) :
				animation->second = adjust_to;
		}

		inline anim_context_t& limit(float max_value) {
			m_original_value = m_value;
			m_limited_min = 0.f;
			m_limited_max = max_value;
			m_is_limited = true;
			return *this;
		}

		inline anim_context_t& limit(float min_value, float max_value) {
			m_original_value = m_value;
			m_limited_min = min_value;
			m_limited_max = max_value;
			m_is_limited = true;
			return *this;
		}

		inline float val() {
			if (m_is_limited) {
				return std::clamp(m_value, m_limited_min, m_value * m_limited_max);
			}
			return m_value;
		}

		inline float normalize() {
			return m_value * 255.f;
		}

		inline void restore() {
			m_is_limited = false;
			m_limited_min = 0.f;
			m_limited_max = 1.f;
		}
	};

	struct vector_anim_context_t {
		math::c_vector_2d m_value;
		unsigned int m_id;
	};

	struct animation_base_t {
		std::string m_cur_child;
		int group_id;

		inline anim_context_t build(std::string string) {
			unsigned int hash = static_cast<unsigned int>(std::hash<std::string>()(string));

			auto animation = stack.find(hash);
			if (animation == stack.end()) {
				stack.insert({ hash, 0.f });
				animation = stack.find(hash);
			}

			return { animation->second, hash };
		}

		inline vector_anim_context_t build_vector_stack(std::string string) {
			unsigned int hash = static_cast<unsigned int>(std::hash<std::string>()(string));
			auto animation = vector_stack.find(hash);

			if (animation == vector_stack.end()) {
				vector_stack.insert({ hash, math::c_vector_2d() });
				animation = vector_stack.find(hash);
			}

			return { animation->second, hash };
		}

		inline float delta_time(float scale) {
			return (1.f / 0.2f) * ImGui::GetIO().DeltaTime * scale;
		}
	};
	inline animation_base_t g_anim_base;

	namespace builder {
		inline anim_context_t create_animation_ctx(std::string id, bool target, float speed)
		{
			anim_context_t animation = g_anim_base.build(id);

			/* build animation */
			animation.animate(animation.m_value + 3.f * g_anim_base.delta_time(speed) * (target ? 1.f : -1.f), 0.f, 1.f, true);

			return animation;
		}

		inline anim_context_t create_animation_ctx_to_target(std::string id, float target, float speed)
		{
			anim_context_t animation = g_anim_base.build(id);
			float delta = (target - animation.m_value) * g_anim_base.delta_time(speed);
			animation.animate(animation.m_value + delta, -FLT_MAX, FLT_MAX, true);
			return animation;
		}

		__forceinline float modulate_float(float input, float input_min, float input_max, float output_min, float output_max)
		{
			if (input_max - input_min == 0) return output_min;
			return (input - input_min) / (input_max - input_min) * (output_max - output_min) + output_min;
		}

		__forceinline std::string wstring_to_string(const wchar_t* wstr) {
			std::wstring_convert<std::codecvt_utf8<wchar_t>> converter;
			return converter.to_bytes(wstr);
		}

		template <typename T>
		__forceinline std::string precision(const T a_value, const int n = 3) {
			std::ostringstream out;
			out.precision(n);
			out << std::fixed << a_value;
			return out.str();
		}

		template <typename obj>
		inline std::string get_id(obj data) {
			return std::to_string(data);
		}
	}
}
