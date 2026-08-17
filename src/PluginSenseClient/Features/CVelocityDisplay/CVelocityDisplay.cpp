#include "CVelocityDisplay.hpp"
#include <cstdio>
#include <cmath>
#include <algorithm>

#include <ImGui/imgui.h>
#include <CS2/SDK/SDK.hpp>
#include <CS2/SDK/Interface/IEngineToClient.hpp>
#include <CS2/SDK/Types/CEntityData.hpp>
#include <GameClient/CL_Players.hpp>
#include <PluginSenseClient/Assets/FredokaOne_Regular.hpp>

namespace vel { extern ImFont* g_font; }

namespace menu_state
{
	extern bool velocityText;
	extern bool velocityGraph;
	extern bool keystrokes;
	extern float velocityOffset;
	extern ImVec4 lowSpeed;
	extern ImVec4 midSpeed;
	extern ImVec4 highSpeed;
	extern ImVec4 graphColor;
}

static CVelocityDisplay g_CVelocityDisplay{};

void CVelocityDisplay::Init()
{
}

void CVelocityDisplay::OnFrame()
{
	if ( !menu_state::velocityText && !menu_state::velocityGraph )
		return;

	auto* pPawn = GetCL_Players()->GetLocalPlayerPawn();
	if ( !pPawn )
		return;

	const Vector3& vel = pPawn->m_vecAbsVelocity();
	const float speed = std::sqrtf( vel.m_x * vel.m_x + vel.m_y * vel.m_y );
	m_history[ m_idx % kHistorySize ] = speed;
	m_idx++;
}

void CVelocityDisplay::OnRender( ImDrawList* drawList, int screenW, int screenH )
{
	if ( !menu_state::velocityText && !menu_state::velocityGraph && !menu_state::keystrokes )
		return;
	if ( !drawList )
		return;

	const float cx = static_cast<float>( screenW ) * 0.5f;
	const float offset = menu_state::velocityOffset;

	// Get current speed from latest history entry
	const int lastIdx = ( m_idx - 1 + kHistorySize ) % kHistorySize;
	const float speed = ( m_idx > 0 ) ? m_history[ lastIdx ] : 0.f;

	ImFont* font = vel::g_font ? vel::g_font : ImGui::GetFont();
	const float fontSize = font->FontSize;

	// ---- Measure all element heights ----
	constexpr float kSpacing = 8.f;
	constexpr float kKeystrokesH = 21.f * 2.f + 15.f;  // cellH*2 + gapY
	constexpr float kGraphH = 100.f;

	float textH = 0.f;
	if ( menu_state::velocityText && speed > 0.1f )
		textH = font->CalcTextSizeA( fontSize, FLT_MAX, -1.f, "999" ).y;

	// ---- Calculate baseY: bottom of screen minus offset, then stack upward ----
	float y = static_cast<float>( screenH ) - offset;

	// 1) Keystrokes (bottom)
	if ( menu_state::keystrokes )
	{
		y -= kKeystrokesH;
		RenderKeystrokes( drawList, screenW, y );
		y -= kSpacing;
	}

	// 2) Velocity text
	if ( menu_state::velocityText && textH > 0.f )
	{
		const int iSpeed = static_cast<int>( std::roundf( speed ) );
		const ImVec4& color = iSpeed < 200 ? menu_state::lowSpeed :
			iSpeed < 250 ? menu_state::midSpeed : menu_state::highSpeed;

		char buf[ 32 ];
		snprintf( buf, sizeof( buf ), "%d", iSpeed );

		const ImVec2 textSize = font->CalcTextSizeA( fontSize, FLT_MAX, -1.f, buf );
		const float tx = cx - textSize.x * 0.5f;
		y -= textSize.y;

		const ImU32 col = ImGui::ColorConvertFloat4ToU32( color );
		drawList->AddText( font, fontSize, ImVec2( tx, y ), col, buf );

		y -= kSpacing;
	}

	// 3) Velocity graph (top)
	if ( menu_state::velocityGraph && m_idx > 1 )
	{
		const float graphW = 300.f;
		const float gx = cx - graphW * 0.5f;
		y -= kGraphH;
		const float gy = y;

		drawList->AddRectFilled( ImVec2( gx, gy ), ImVec2( gx + graphW, gy + kGraphH ),
			IM_COL32( 0, 0, 0, 100 ), 4.f );

		float maxSpeed = 300.f;
		const int samples = (std::min)( m_idx, kHistorySize );
		for ( int i = 0; i < samples; ++i ) {
			const float v = m_history[ ( m_idx - 1 - i + kHistorySize ) % kHistorySize ];
			if ( v > maxSpeed ) maxSpeed = v;
		}
		if ( maxSpeed < 50.f ) maxSpeed = 50.f;

		const ImU32 lineCol = ImGui::ColorConvertFloat4ToU32( menu_state::graphColor );
		for ( int i = 1; i < samples; ++i ) {
			const int i0 = ( m_idx - 1 - i + 1 + kHistorySize ) % kHistorySize;
			const int i1 = ( m_idx - 1 - i + kHistorySize ) % kHistorySize;
			const float v0 = m_history[ i0 ] / maxSpeed;
			const float v1 = m_history[ i1 ] / maxSpeed;
			const float x0 = gx + graphW - ( float( i - 1 ) / ( samples - 1 ) ) * graphW;
			const float x1 = gx + graphW - ( float( i ) / ( samples - 1 ) ) * graphW;
			drawList->AddLine( ImVec2( x0, gy + kGraphH - v0 * kGraphH ),
				ImVec2( x1, gy + kGraphH - v1 * kGraphH ), lineCol, 2.f );
		}

		// threshold line at 250
		const float threshY = gy + kGraphH - ( 250.f / maxSpeed ) * kGraphH;
		if ( threshY > gy && threshY < gy + kGraphH )
			drawList->AddLine( ImVec2( gx, threshY ), ImVec2( gx + graphW, threshY ),
				IM_COL32( 255, 200, 50, 80 ), 1.5f );
	}
}

void CVelocityDisplay::RenderKeystrokes( ImDrawList* drawList, int screenW, float topY )
{
	struct KeyDef { const char* label; int vk; };
	static constexpr KeyDef row0[] = {
		{ "C", VK_CONTROL },
		{ "W", 'W' },
		{ "J", VK_SPACE },
	};
	static constexpr KeyDef row1[] = {
		{ "A", 'A' },
		{ "S", 'S' },
		{ "D", 'D' },
	};

	const float cellW = 28.f;
	const float cellH = 24.f;
	const float gapY = 15.f;
	const float totalW = cellW * 3.f;

	const float baseX = ( static_cast<float>( screenW ) - totalW ) * 0.5f;

	ImFont* font = vel::g_font ? vel::g_font : ImGui::GetFont();
	const float fontSize = font->FontSize;

	auto drawKey = [&]( float x, float y, const KeyDef& key ) {
		bool pressed = ( GetAsyncKeyState( key.vk ) & 0x8000 ) != 0;
		const char* text = pressed ? key.label : "_";
		ImVec2 ts = font->CalcTextSizeA( fontSize, FLT_MAX, -1.f, text );
		float tx = x + ( cellW - ts.x ) * 0.5f;
		float ty = y + ( cellH - ts.y ) * 0.5f;
		drawList->AddText( font, fontSize, ImVec2( tx + 1.f, ty + 1.f ),
			IM_COL32( 0, 0, 0, 120 ), text );
		drawList->AddText( font, fontSize, ImVec2( tx, ty ),
			pressed ? IM_COL32( 255, 255, 255, 255 )
					: IM_COL32( 180, 180, 180, 170 ), text );
	};

	for ( int i = 0; i < 3; ++i ) {
		drawKey( baseX + i * cellW, topY, row0[i] );
		drawKey( baseX + i * cellW, topY + cellH + gapY, row1[i] );
	}
}

auto GetVelocityDisplay() -> CVelocityDisplay*
{
	return &g_CVelocityDisplay;
}
