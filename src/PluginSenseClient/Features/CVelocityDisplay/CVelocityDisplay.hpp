#pragma once

struct ImDrawList;
struct ImFont;
class CUserCmd;

class CVelocityDisplay final
{
public:
	void Init();
	void OnFrame();
	void OnRender( ImDrawList* drawList, int screenW, int screenH );
	void OnCreateMove( CUserCmd* pCmd );

	static constexpr int kHistorySize = 128;

private:
	void RenderKeystrokes( ImDrawList* drawList, int screenW, float topY );

	float m_history[kHistorySize] = {};
	int m_idx = 0;
	CUserCmd* m_pCmd = nullptr;
};

auto GetVelocityDisplay() -> CVelocityDisplay*;
