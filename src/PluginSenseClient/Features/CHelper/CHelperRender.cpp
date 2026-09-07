#include "CHelper.hpp"
#include "CHelperDetail.hpp"

#include <cmath>
#include <cstdio>
#include <numbers>

#include <PluginSenseClient/Settings/MenuState.hpp>
#include <PluginSenseClient/GUI/framework_w/includes.hh>

#include <GameClient/CL_Players.hpp>

namespace nd = resources::nades;

namespace
{
	// esp_icons 投掷物图标字符
	const char* HelperKindIcon( std::uint8_t kind )
	{
		switch ( static_cast<nd::kind>( kind ) )
		{
		case nd::kind::flash:   return "\x63";  // 闪光弹
		case nd::kind::he:      return "\x64";  // 手雷
		case nd::kind::smoke:   return "\x65";  // 烟雾弹
		case nd::kind::molotov: return "\x66";  // 燃烧弹
		case nd::kind::decoy:   return "\x26";  // 诱饵弹
		default:                return nullptr;
		}
	}

	ImU32 AccentColor( float alpha )
	{
		return IM_COL32(
			static_cast<int>( vars::colorAccent[ 0 ] * 255.f ) ,
			static_cast<int>( vars::colorAccent[ 1 ] * 255.f ) ,
			static_cast<int>( vars::colorAccent[ 2 ] * 255.f ) ,
			static_cast<int>( alpha ) );
	}
}

// 名牌图标字符:墙点画"当前手持武器"的图标(Collect 已按当前枪过滤,显示即匹配);
// 投掷点不在这走(投掷图标由 HelperKindIcon 提供)
std::string CHelper::LineupIconChar( const LineupView& lineup ) const
{
	if ( lineup.kind == static_cast<std::uint8_t>( nd::kind::wallbang ) )
		return WeaponIconChar( WeaponShortName( ActiveWeaponItem() ) );
	return {};
}

void CHelper::DrawStandMarker( ImDrawList* drawList , const LineupView& lineup , bool standing ) const
{
	// 近(stand_radius 内)主题色,远(stand_distance 内)白色
	const ImU32 color = standing ? AccentColor( 255 ) : IM_COL32( 245 , 245 , 250 , 235 );

	constexpr int segments{ 28 };
	std::vector<ImVec2> points;
	points.reserve( segments );

	for ( int i = 0; i < segments; ++i )
	{
		const float theta = ( static_cast<float>( i ) / segments ) * 2.0f * std::numbers::pi_v<float>;
		const Vector3 world{
			lineup.position.m_x + std::cosf( theta ) * menu_state::standRadius ,
			lineup.position.m_y + std::sinf( theta ) * menu_state::standRadius ,
			lineup.position.m_z + 1.0f ,
		};
		ImVec2 screen;
		if ( !Math::WorldToScreen( world , screen ) )
			return;
		points.push_back( screen );
	}

	if ( points.size() > 1 )
		drawList->AddPolyline( points.data() , static_cast<int>( points.size() ) , color , true , standing ? 2.0f : 1.4f );

	ImVec2 center;
	if ( Math::WorldToScreen( lineup.position + Vector3{ 0.f , 0.f , 1.f } , center ) )
		drawList->AddCircleFilled( center , standing ? 2.5f : 1.8f , color , 10 );
}

// 描点(背景圈+瞄点圈+名称/action)
void CHelper::DrawMouseAimPoints( ImDrawList* drawList , int screenW , int screenH ) const
{
	if ( m_RenderScratch.empty() )
		return;

	QAngle viewAngles;
	if ( !GetRenderCameraAngles( viewAngles ) )
		return;

	const Vector3 eyePos = GetCL_Players()->GetLocalEyeOrigin();
	const Vector3 playerPos = GetCL_Players()->GetLocalPlayerPawn()->GetOrigin();

	const ImU32 themeCol = AccentColor( 255 );
	const ImU32 grayCol = IM_COL32( 180 , 180 , 180 , 200 );
	const ImU32 grayTextCol = IM_COL32( 220 , 220 , 220 , 200 );

	// 第一遍:收集瞄点屏幕位置 + 就绪状态
	struct AimScreen
	{
		const LineupView* lineup;
		ImVec2 screen;
		bool converged;
	};
	std::vector<AimScreen> aims;

	for ( std::size_t i = 0; i < m_RenderScratch.size(); ++i )
	{
		const auto& lineup = m_RenderScratch[ i ];
		if ( lineup.distance > menu_state::standRadius )
			continue;

		// 就绪 = 精确站位(release_radius 内) + 瞄准到位(误差 <= aim_threshold)
		const bool converged = ExecutionPositionReady( lineup , playerPos )
			&& AngleError( viewAngles , lineup.pitch , lineup.yaw ) <= menu_state::aimThreshold;

		Vector3 forward;
		const QAngle angles{ lineup.pitch , lineup.yaw , 0.f };
		Math::AngleVectors( angles , forward );

		// 从玩家眼睛沿投掷方向 220u(跟准星方向一致)
		const Vector3 worldAim = eyePos + forward * 220.f;

		ImVec2 screen;
		if ( !Math::WorldToScreen( worldAim , screen ) )
			continue;
		if ( screen.x < 0.f || screen.x > (float)screenW || screen.y < 0.f || screen.y > (float)screenH )
			continue;

		aims.push_back( { &lineup , screen , converged } );
	}

	// 第二遍:绘制
	for ( const auto& a : aims )
	{
		const bool converged = a.converged;

		// 描点圈半径随 aim_threshold(度)线性放大:阈值越大允许误差越大,圈越大
		const float ringRadius = 4.f + menu_state::aimThreshold * 6.f;

		// 背景圈 + 瞄点圈(就绪主题色,未就绪灰色)
		drawList->AddCircleFilled( a.screen , ringRadius + 2.f , IM_COL32( 0 , 0 , 0 , 64 ) , 16 );
		const ImU32 ring = converged ? themeCol : grayCol;
		drawList->AddCircle( a.screen , ringRadius , ring , 16 , converged ? 2.f : 1.f );

		// 描点卡片(黑底圆角 + 名称/action,就绪主题色/未就绪灰色)
		ImFont* font = g_font->f_childs.get_font();
		const float fontSize = font ? font->FontSize : 14.f;
		if ( font )
		{
			const std::string name = a.lineup->name.empty() ? "?" : a.lineup->name;
			// 墙点标注(Crouch/Jump)副标题
			std::string action;
			if ( a.lineup->kind == static_cast<std::uint8_t>( nd::kind::wallbang ) )
			{
				if ( a.lineup->annotations & nd::action_crouch )
					action = "Crouch";
				if ( a.lineup->annotations & nd::action_jump )
					action = action.empty() ? "Jump" : action + "+Jump";
			}
			const ImU32 textCol = converged ? themeCol : grayTextCol;

			const ImVec2 nameSize = font->CalcTextSizeA( fontSize , FLT_MAX , -1.f , name.c_str() );
			const ImVec2 actionSize = !action.empty()
				? font->CalcTextSizeA( fontSize - 2.f , FLT_MAX , -1.f , action.c_str() )
				: ImVec2{};

			const float textW = std::max( nameSize.x , actionSize.x );
			const float textH = nameSize.y + ( actionSize.y > 0.f ? 2.f + actionSize.y : 0.f );
			const float padX = 5.f;
			const float padY = 3.f;
			const ImVec2 boxPos( a.screen.x + 14.f - padX , a.screen.y - 9.f - padY );
			const ImVec2 boxEnd( boxPos.x + textW + padX * 2.f , boxPos.y + textH + padY * 2.f );
			drawList->AddRectFilled( boxPos , boxEnd , IM_COL32( 0 , 0 , 0 , 150 ) , 3.f );

			float ty = boxPos.y + padY;
			drawList->AddText( font , fontSize , { boxPos.x + padX , ty } , textCol , name.c_str() );
			ty += nameSize.y + 2.f;
			if ( !action.empty() )
				drawList->AddText( font , fontSize - 2.f , { boxPos.x + padX , ty } , textCol , action.c_str() );
		}
	}
}

// 渲染主入口
auto CHelper::OnRender( ImDrawList* drawList , int screenW , int screenH ) -> void
{
	if ( !drawList )
		return;

	Tick();

	// 录制状态卡片(与 helper 开关无关,录制会话中始终显示)
	DrawRecordStatus( drawList , screenW , screenH );

	auto* player = GetCL_Players()->GetLocalPlayerPawn();
	if ( !player || !player->IsAlive() || !menu_state::helperEnabled )
		return;

	const Vector3 playerPos = player->GetOrigin();
	const std::uint8_t kind = ResolveWeaponKind();
	if ( !Collect( playerPos , kind , m_RenderScratch ) )
		return;

	// 点位名牌(Lua 方式:同投掷物 + 位置 20u 内 = 同一个站位,合并成一个名牌,名字垂直堆叠)
	struct Group
	{
		std::uint8_t kind;
		Vector3 anchor;
		std::vector<const LineupView*> points;
	};
	std::vector<Group> groups;
	for ( const auto& point : m_RenderScratch )
	{
		Group* found = nullptr;
		for ( auto& g : groups )
		{
			if ( g.kind == point.kind && ( g.anchor - point.position ).Length() <= 20.f )
			{
				found = &g;
				break;
			}
		}
		if ( !found )
		{
			groups.push_back( { point.kind , point.position , {} } );
			found = &groups.back();
		}
		found->points.push_back( &point );
	}

	for ( const auto& g : groups )
	{
		// 组中心投影(名牌位置)
		Vector3 center;
		for ( const auto* p : g.points )
			center += p->position;
		center *= 1.f / static_cast<float>( g.points.size() );
		center.m_z += 8.f;

		ImVec2 screen;
		if ( !Math::WorldToScreen( center , screen ) )
			continue;
		if ( screen.x < 0.f || screen.x > (float)screenW || screen.y < 0.f || screen.y > (float)screenH )
			continue;

		ImFont* font = g_font->f_childs.get_font();
		const float fontSize = font ? font->FontSize : 14.f;

		// 图标(投掷物;墙点组用组内首点的武器图标)——先量宽再布局
		const char* icon = g.kind == static_cast<std::uint8_t>( nd::kind::wallbang )
			? nullptr : HelperKindIcon( g.kind );
		const std::string weaponIcon = !g.points.empty()
			? LineupIconChar( *g.points.front() ) : std::string();
		ImFont* iconFont = g_font ? g_font->f_weapon_icons.get_font() : nullptr;
		const float iconFontSize = 14.f;
		const char* iconText = icon ? icon : weaponIcon.c_str();
		const bool hasIcon = ( icon || !weaponIcon.empty() ) && iconFont;

		// 图标槽:宽度基准 18,字形更宽随之加宽;高度固定 16,不随字形撑高名牌。
		// 枪械字形(墙点)宽高比大,按目标宽 22 反算字号,不同枪的图标宽度保持一致。
		const float iconBase = 18.f;
		const float iconH = 16.f;
		float drawIconSize = iconFontSize;
		float iconW = iconBase;
		ImVec2 iconTextSize{};
		if ( hasIcon )
		{
			iconTextSize = iconFont->CalcTextSizeA( drawIconSize , FLT_MAX , -1.f , iconText );
			if ( !icon && iconTextSize.x > 22.f )
			{
				drawIconSize = std::max( 8.f , iconFontSize * 22.f / iconTextSize.x );
				iconTextSize = iconFont->CalcTextSizeA( drawIconSize , FLT_MAX , -1.f , iconText );
			}
			iconW = std::max( iconBase , iconTextSize.x + 6.f );
		}

		// 名牌尺寸:图标槽 + 分隔线 + 垂直堆叠的名字(带距离)
		const float padX = 6.f;
		const float padY = 4.f;

		float maxTextW = 0.f;
		float totalH = 0.f;
		std::vector<ImVec2> sizes;
		for ( const auto* p : g.points )
		{
			std::string name = p->name.empty() ? "?" : p->name;
			if ( menu_state::showDistance )
			{
				char buf[ 32 ];
				std::snprintf( buf , sizeof( buf ) , "  %.0fm" , p->distance / 52.0f );
				name += buf;
			}
			const ImVec2 sz = font
				? font->CalcTextSizeA( fontSize , FLT_MAX , -1.f , name.c_str() )
				: ImVec2( 60.f , 16.f );
			sizes.push_back( sz );
			maxTextW = std::max( maxTextW , sz.x );
			totalH += std::max( 0.f , sz.y - 1.f );
		}

		const float cardW = iconW + 6.f + maxTextW + padX * 2.f + 8.f;
		const float cardH = std::max( iconH , totalH ) + padY * 2.f;

		const float left = screen.x - cardW * 0.5f;
		const float top = screen.y - cardH - 18.f;
		const float iconLeft = left + padX;
		const float iconTop = top + ( cardH - iconH ) * 0.5f;
		const float dividerX = iconLeft + iconW + 3.f;

		const ImU32 base = AccentColor( 255 );
		drawList->AddRectFilled( { left , top } , { left + cardW , top + cardH } , IM_COL32( 0 , 0 , 0 , 150 ) , 4.f );

		if ( hasIcon )
		{
			drawList->AddText( iconFont , drawIconSize ,
				{ iconLeft + ( iconW - iconTextSize.x ) * 0.5f , iconTop + ( iconH - iconTextSize.y ) * 0.5f } ,
				base , iconText );
		}
		else
		{
			drawList->AddRectFilled( { iconLeft , iconTop } , { iconLeft + iconW , iconTop + iconH } , base , 2.f );
		}

		// 分隔线
		drawList->AddRectFilled( { dividerX , top + 2.f } , { dividerX + 2.f , top + cardH - 2.f } , base , 1.f );

		// 垂直堆叠名字
		float y = top + ( cardH - totalH ) * 0.5f;
		for ( std::size_t i = 0; i < g.points.size(); ++i )
		{
			std::string name = g.points[ i ]->name.empty() ? "?" : g.points[ i ]->name;
			if ( menu_state::showDistance )
			{
				char buf[ 32 ];
				std::snprintf( buf , sizeof( buf ) , "  %.0fm" , g.points[ i ]->distance / 52.0f );
				name += buf;
			}
			drawList->AddText( font , fontSize , { dividerX + 6.f , y } , IM_COL32( 245 , 247 , 252 , 255 ) , name.c_str() );
			y += std::max( 0.f , sizes[ i ].y - 1.f );
		}
	}

	// 站圈(参考 vesta:stand_radius 内绿,stand_distance 内蓝,更远不画)
	for ( const auto& lineup : m_RenderScratch )
	{
		if ( lineup.distance > menu_state::standDistance )
			continue;
		DrawStandMarker( drawList , lineup , lineup.distance <= menu_state::standRadius );
	}

	// 描点(当前走到的点位对应的所有描点)
	DrawMouseAimPoints( drawList , screenW , screenH );
}
