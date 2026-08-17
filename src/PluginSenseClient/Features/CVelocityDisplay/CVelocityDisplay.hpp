#pragma once

struct ImDrawList;
struct ImFont;

class CVelocityDisplay final
{
public:
	void Init();
	void OnFrame();
	void OnRender( ImDrawList* drawList, int screenW, int screenH );

	static constexpr int kHistorySize = 128;

private:
	void RenderKeystrokes( ImDrawList* drawList, int screenW, float topY );

	float m_history[kHistorySize] = {};
	int m_idx = 0;
};

auto GetVelocityDisplay() -> CVelocityDisplay*;
