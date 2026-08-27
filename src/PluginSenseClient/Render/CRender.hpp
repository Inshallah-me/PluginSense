#pragma once

#include <Common/Common.hpp>
#include <ImGui/imgui.h>

#include <CS2/SDK/Math/Math.hpp>

struct ID3D11ShaderResourceView;

class CRender final
{
public:
	// HUD 统一绘制层:游戏内 HUD 都画在背景层(与菜单同层,绘制顺序决定上下,
	// 菜单在 CPluginSenseClient::OnRender 最后绘制,始终位于 HUD 之上)。
	// 所有 HUD 功能应通过此入口拿 draw list,避免直接使用前景层盖住菜单。
	static auto GetHudDrawList() -> ImDrawList*;

	auto DrawLine( const ImVec2& Start , const ImVec2& End , const ImColor& Color , float thickness = 1.f ) -> void;
	auto DrawBox( const ImVec2& Min , const ImVec2& Max , const ImColor& Color ) -> void;
	auto DrawOutlineBox( const ImVec2& Min , const ImVec2& Max , const ImColor& Color ) -> void;
	auto DrawCoalBox( const ImVec2& Min , const ImVec2& Max , const ImColor& Color ) -> void;
	auto DrawOutlineCoalBox( const ImVec2& Min , const ImVec2& Max , const ImColor& Color ) -> void;
	auto DrawFillBox( const ImVec2& Min , const ImVec2& Max , const ImColor& Color ) -> void;
	auto DrawRectFilledMultiColor( const ImVec2& Min , const ImVec2& Max , const ImU32 col_upr_left , const ImU32 col_upr_right , const ImU32 col_bot_right , const ImU32 col_bot_left ) -> void;
	auto DrawCircle( const ImVec2& Center , float radius , const ImColor& Color ) -> void;
	auto DrawCircleFilled( const ImVec2& Center , float radius , const ImColor& Color ) -> void;
	auto DrawCircle3D( const Vector3& Center , float radius , const ImColor& Color ) -> void;
	auto DrawTriangleFilled( const ImVec2& Center , const ImVec2& Pos1 , const ImVec2& Pos2 , const ImColor& Color ) -> void;

public:
	auto DrawImage( ID3D11ShaderResourceView* pD3D11ShaderResourceView , const float x , const float y , const float w , const float h ) -> void;
};

auto GetRender() -> CRender*;
