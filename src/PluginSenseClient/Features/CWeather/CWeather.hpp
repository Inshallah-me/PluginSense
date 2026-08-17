#pragma once

#include <cstdint>
#include <string>

class CWeather final
{
public:
	void Init();
	void on_frame_stage_notify();
	void release();

private:
	static constexpr std::uint32_t invalid_effect_index{ static_cast<std::uint32_t>( -1 ) };

	void create_particle();
	void update_particles();
	void release_particles();

	std::uint32_t m_effect_index{ invalid_effect_index };
	int m_last_particle_type{ -1 };
	float m_last_round_start_time{};
	bool m_particle_loaded{};

	struct buffer_string
	{
		std::uint32_t m_unknown1{};
		std::uint32_t m_unknown2{ 0xc00000c8 };

		union
		{
			std::uintptr_t m_str_ptr;
			std::uint8_t data[0xc8];
		};

		std::uintptr_t m_unknown3{};
		std::uintptr_t m_unknown4{};
	};
};

auto GetWeather() -> CWeather*;
