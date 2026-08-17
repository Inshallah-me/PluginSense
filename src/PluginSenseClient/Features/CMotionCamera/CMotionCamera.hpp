#pragma once

#include <cstdint>

struct ImDrawList;

class CMotionCamera final
{
public:
	void Init();
	void on_create_move( class CCSGOInput* pInput );
	void on_override_view( std::uintptr_t view_setup );
	void on_render( ImDrawList* drawList, int screenW, int screenH );

private:
	float m_cam_x = 0.f;
	float m_cam_y = 0.f;
	float m_cam_z = 0.f;
	float m_view_pitch = 0.f;
	float m_view_yaw = 0.f;
	float m_cross_x = 0.f;
	float m_cross_y = 0.f;
	bool m_cross_valid = false;
	bool m_initialized = false;
	bool m_is_thirdperson = false;
};

auto GetMotionCamera() -> CMotionCamera*;
