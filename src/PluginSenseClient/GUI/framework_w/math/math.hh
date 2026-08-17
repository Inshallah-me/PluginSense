#pragma once

namespace hue {
	class c_color {
	public:
		c_color() : r(255), g(255), b(255), a(255) {}
		c_color(int r, int g, int b, int a = 255) : r(r), g(g), b(b), a(a) {}
		c_color(int r, int g, int b, float a) : r(r), g(g), b(b), a(static_cast<int>(a)) {}
		c_color(float r, float g, float b, int a = 255)
			: r(static_cast<int>(r)), g(static_cast<int>(g)), b(static_cast<int>(b)), a(a) {}
		c_color(float r, float g, float b, float a)
			: r(static_cast<int>(r)), g(static_cast<int>(g)), b(static_cast<int>(b)), a(static_cast<int>(a)) {}
		~c_color() {}

		// transform color
		std::uint32_t transform() {
			std::uint32_t out = 0;

			out = static_cast<std::uint32_t>(this->r) << 0;
			out |= static_cast<std::uint32_t>(this->g) << 8;
			out |= static_cast<std::uint32_t>(this->b) << 16;
			out |= static_cast<std::uint32_t>(this->a) << 24;

			return out;
		}

		c_color hex(std::string hex_color) {
			if (hex_color[0] == '#') {
				hex_color.erase(0, 1);
			}

			std::stringstream ss;
			ss << std::hex << hex_color;
			unsigned int hex_value;
			ss >> hex_value;

			int red = (hex_value >> 16) & 0xff;
			int green = (hex_value >> 8) & 0xff;
			int blue = hex_value & 0xff;

			return c_color(red, green, blue);
		}

		c_color modulate(float anim) {
			return c_color(r, g, b, static_cast<int>(anim * 255.f));
		}

		c_color modulate_normal(int anim) {
			return c_color(r, g, b, anim);
		}

		c_color blend(c_color _tmp, float fraction, float alpha = 1.f) {
			float r_d = static_cast<float>(r - _tmp.r);
			float g_d = static_cast<float>(g - _tmp.g);
			float b_d = static_cast<float>(b - _tmp.b);

			return c_color(
				static_cast<int>(r - (r_d * fraction)),
				static_cast<int>(g - (g_d * fraction)),
				static_cast<int>(b - (b_d * fraction)),
				static_cast<int>(alpha));
		}

		c_color lerp(const c_color& target, float t) const {
			return c_color(
				static_cast<int>(r + (target.r - r) * t),
				static_cast<int>(g + (target.g - g) * t),
				static_cast<int>(b + (target.b - b) * t),
				static_cast<int>(a + (target.a - a) * t)
			);
		}

		c_color with_alpha(int alpha) const { return { r, g, b, alpha }; }

		static c_color from_hsv(float h, float s, float v, int a = 255) {
			float c = v * s;
			float x = c * (1 - std::abs(std::fmod(h / 60.f, 2.f) - 1));
			float m = v - c;
			float r, g, b;

			if (h < 60) { r = c; g = x; b = 0; }
			else if (h < 120) { r = x; g = c; b = 0; }
			else if (h < 180) { r = 0; g = c; b = x; }
			else if (h < 240) { r = 0; g = x; b = c; }
			else if (h < 300) { r = x; g = 0; b = c; }
			else { r = c; g = 0; b = x; }

			return c_color(
				static_cast<int>((r + m) * 255),
				static_cast<int>((g + m) * 255),
				static_cast<int>((b + m) * 255),
				a
			);
		}

#undef max
#undef min
		void to_hsv(float& h, float& s, float& v) const {
			float rf = r / 255.f, gf = g / 255.f, bf = b / 255.f;
			float max_val = std::max({ rf, gf, bf });
			float min_val = std::min({ rf, gf, bf });
			float delta = max_val - min_val;

			v = max_val;
			s = max_val > 0 ? delta / max_val : 0;

			if (delta == 0) h = 0;
			else if (max_val == rf) h = 60.f * std::fmod((gf - bf) / delta, 6.f);
			else if (max_val == gf) h = 60.f * ((bf - rf) / delta + 2);
			else h = 60.f * ((rf - gf) / delta + 4);

			if (h < 0) h += 360;
		}

		// colorpicker stuff
		struct hsv_context_t {
			float hue, saturation, value;
		};

		static c_color hsv_to_rgb(float hue, float sat, float val) {
			float red, grn, blu;
			float i = 0.f, f = 0.f, p = 0.f, q = 0.f, t = 0.f;
			c_color result;

			if (val == 0) {
				red = 0;
				grn = 0;
				blu = 0;
			}
			else {
				hue /= 60;
				i = std::floor(hue);
				f = hue - i;
				p = val * (1 - sat);
				q = val * (1 - (sat * f));
				t = val * (1 - (sat * (1 - f)));
				if (i == 0) {
					red = val;
					grn = t;
					blu = p;
				}
				else if (i == 1) {
					red = q;
					grn = val;
					blu = p;
				}
				else if (i == 2) {
					red = p;
					grn = val;
					blu = t;
				}
				else if (i == 3) {
					red = p;
					grn = q;
					blu = val;
				}
				else if (i == 4) {
					red = t;
					grn = p;
					blu = val;
				}
				else if (i == 5) {
					red = val;
					grn = p;
					blu = q;
				}
			}

			result = c_color(int(red * 255), int(grn * 255), int(blu * 255));
			return result;
		}

		static hsv_context_t rgb_to_hsv(c_color a) {
			float red, grn, blu;
			red = (float)a.r / 255.f;
			grn = (float)a.g / 255.f;
			blu = (float)a.b / 255.f;
			float hue, sat, val;
			float x, f, i;
			hsv_context_t result;

			x = std::fmin(std::fmin(red, grn), blu);
			val = std::fmax(std::fmax(red, grn), blu);
			if (x == val) {
				hue = 0;
				sat = 0;
			}
			else {
				f = (red == x) ? grn - blu : ((grn == x) ? blu - red : red - grn);
				i = (red == x) ? 3.f : ((grn == x) ? 5.f : 1.f);
				hue = std::fmod((i - f / (val - x)) * 60.f, 360.f);
				sat = ((val - x) / val);
			}
			result.hue = hue;
			result.saturation = sat;
			result.value = val;

			return result;
		}

		struct hsv_color {
			float h, s, v;
		};

#undef min
#undef max

		hsv_color rgb_to_hsv(int r, int g, int b) const {
			float rf = r / 255.0f;
			float gf = g / 255.0f;
			float bf = b / 255.0f;

			float max_val = std::max({ rf, gf, bf });
			float min_val = std::min({ rf, gf, bf });
			float delta = max_val - min_val;

			float h = 0.0f;
			if (delta != 0.0f) {
				if (max_val == rf) {
					h = 60.0f * (fmodf((gf - bf) / delta, 6.0f));
				}
				else if (max_val == gf) {
					h = 60.0f * ((bf - rf) / delta + 2.0f);
				}
				else {
					h = 60.0f * ((rf - gf) / delta + 4.0f);
				}
			}
			if (h < 0.0f) h += 360.0f;

			float s = (max_val == 0.0f) ? 0.0f : delta / max_val;
			float v = max_val;

			return { h, s, v };
		}

		void hsv_to_rgb(const hsv_color& hsv, int& out_r, int& out_g, int& out_b) const {
			float c = hsv.v * hsv.s;
			float x = c * (1.0f - fabsf(fmodf(hsv.h / 60.0f, 2.0f) - 1.0f));
			float m = hsv.v - c;

			float rf = 0.0f, gf = 0.0f, bf = 0.0f;

			if (0.0f <= hsv.h && hsv.h < 60.0f) {
				rf = c; gf = x; bf = 0.0f;
			}
			else if (60.0f <= hsv.h && hsv.h < 120.0f) {
				rf = x; gf = c; bf = 0.0f;
			}
			else if (120.0f <= hsv.h && hsv.h < 180.0f) {
				rf = 0.0f; gf = c; bf = x;
			}
			else if (180.0f <= hsv.h && hsv.h < 240.0f) {
				rf = 0.0f; gf = x; bf = c;
			}
			else if (240.0f <= hsv.h && hsv.h < 300.0f) {
				rf = x; gf = 0.0f; bf = c;
			}
			else if (300.0f <= hsv.h && hsv.h < 360.0f) {
				rf = c; gf = 0.0f; bf = x;
			}

			out_r = static_cast<int>((rf + m) * 255.0f);
			out_g = static_cast<int>((gf + m) * 255.0f);
			out_b = static_cast<int>((bf + m) * 255.0f);
		}

		c_color darken(float factor) const {
			if (factor < 0.0f) factor = 0.0f;
			if (factor > 1.0f) factor = 1.0f;

			int new_r = static_cast<int>(r * factor);
			int new_g = static_cast<int>(g * factor);
			int new_b = static_cast<int>(b * factor);

			new_r = std::max(0, std::min(255, new_r));
			new_g = std::max(0, std::min(255, new_g));
			new_b = std::max(0, std::min(255, new_b));

			if (factor > 0.0f && new_r == 0 && new_g == 0 && new_b == 0 && (r > 0 || g > 0 || b > 0)) {
				new_r = static_cast<int>(r * 0.1f);
				new_g = static_cast<int>(g * 0.1f);
				new_b = static_cast<int>(b * 0.1f);
				new_r = std::max(1, std::min(255, new_r));
				new_g = std::max(1, std::min(255, new_g));
				new_b = std::max(1, std::min(255, new_b));
			}

			return c_color(new_r, new_g, new_b, a);
		}


		int r, g, b, a;
	};
}

namespace math {

	class c_vector_2d {
	public:
		/* default builders */
		c_vector_2d() : x(0), y(0) {}
		c_vector_2d(float x, float y) : x(x), y(y) {}
		c_vector_2d(int x, int y) : x(static_cast<float>(x)), y(static_cast<float>(y)) {}
		c_vector_2d(float x, int y) : x(x), y(static_cast<float>(y)) {}
		c_vector_2d(int x, float y) : x(static_cast<float>(x)), y(y) {}
		~c_vector_2d() {}

		bool operator==(const c_vector_2d& src) const {
			return (src.x == this->x) && (src.y == y);
		}

		bool operator!=(const c_vector_2d& src) const {
			return (src.x != this->x) || (src.y != y);
		}

		float& operator[](int i) {
			return ((float*)this)[i];
		}

		float operator[](int i) const {
			return ((float*)this)[i];
		}

		c_vector_2d& operator+=(const c_vector_2d& v) {
			this->x += v.x; this->y += v.y;
			return *this;
		}

		c_vector_2d& operator-=(const c_vector_2d& v) {
			this->x -= v.x; this->y -= v.y;
			return *this;
		}

		c_vector_2d& operator*=(float fl) {
			this->x *= fl;
			this->y *= fl;
			return *this;
		}

		c_vector_2d& operator*=(const c_vector_2d& v) {
			this->x *= v.x;
			this->y *= v.y;
			return *this;
		}

		c_vector_2d& operator/=(const c_vector_2d& v) {
			this->x /= v.x;
			this->y /= v.y;
			return *this;
		}

		c_vector_2d& operator+=(float fl) {
			this->x += fl;
			this->y += fl;
			return *this;
		}

		c_vector_2d& operator/=(float fl) {
			this->x /= fl;
			this->y /= fl;
			return *this;
		}

		c_vector_2d& operator-=(float fl) {
			this->x -= fl;
			this->y -= fl;
			return *this;
		}

		c_vector_2d operator+(const c_vector_2d& v) const {
			return c_vector_2d(this->x + v.x, this->y + v.y);
		}

		c_vector_2d operator*(const c_vector_2d& v) const {
			return c_vector_2d(this->x * v.x, this->y * v.y);
		}

		c_vector_2d operator*(float v) const {
			return c_vector_2d(this->x * v, this->y * v);
		}

		c_vector_2d operator-(const c_vector_2d& v) const {
			return c_vector_2d(this->x - v.x, this->y - v.y);
		}

		c_vector_2d operator/(const c_vector_2d& v) const {
			return c_vector_2d(this->x / v.x, this->y / v.y);
		}

		c_vector_2d operator/(float v) const {
			return c_vector_2d(this->x / v, this->y / v);
		}

		float length_2d() const {
			return std::sqrt(x * x + y * y);
		}

		ImVec2 transform() {
			return ImVec2(this->x, this->y);
		}

		/* parameters */
		float x;
		float y;
	};

	class c_rect {
	public:
		float x, y, w, h;

		c_rect() : x(0), y(0), w(0), h(0) {}
		c_rect(float x, float y, float w, float h) : x(x), y(y), w(w), h(h) {}
		c_rect(c_vector_2d pos, c_vector_2d size) : x(pos.x), y(pos.y), w(size.x), h(size.y) {}
		~c_rect() {}

		bool contains(c_vector_2d point) const {
			return point.x >= x && point.x <= x + w && point.y >= y && point.y <= y + h;
		}

		bool contains(float px, float py) const {
			return px >= x && px <= x + w && py >= y && py <= y + h;
		}


		/* position */
		inline math::c_vector_2d pos() {
			return math::c_vector_2d(x, y);
		}

		inline math::c_vector_2d size() {
			return math::c_vector_2d(w, h);
		}
	};


	struct rect_t {
		float x, y, w, h;

		rect_t() : x(0), y(0), w(0), h(0) {}
		rect_t(float x, float y, float w, float h) : x(x), y(y), w(w), h(h) {}
		rect_t(c_vector_2d pos, c_vector_2d size) : x(pos.x), y(pos.y), w(size.x), h(size.y) {}

		bool contains(c_vector_2d point) const {
			return point.x >= x && point.x <= x + w && point.y >= y && point.y <= y + h;
		}

		bool contains(float px, float py) const {
			return px >= x && px <= x + w && py >= y && py <= y + h;
		}

		c_vector_2d position() const { return c_vector_2d(x, y); }
		c_vector_2d size() const { return c_vector_2d(w, h); }
		c_vector_2d center() const { return c_vector_2d(x + w / 2, y + h / 2); }

		float right() const { return x + w; }
		float bottom() const { return y + h; }
	};
}
