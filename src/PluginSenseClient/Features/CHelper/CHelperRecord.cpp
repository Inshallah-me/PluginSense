#include "CHelper.hpp"
#include "CHelperDetail.hpp"
#include "CHelperRecorder.hpp"

#include <cstdio>
#include <cmath>

#include <Common/DevLog.hpp>

#include <PluginSenseClient/Settings/MenuState.hpp>
#include <PluginSenseClient/GUI/framework_w/includes.hh>

namespace nd = resources::nades;

// ============================================================================
// 菜单点位表:用户录制列表
// ============================================================================
std::vector<std::string> CHelper::BuildRecorderItems( std::uint8_t kindFilter ) const
{
	std::vector<std::string> items;
	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() )
		return items;

	if ( auto* recorder = GetHelperRecorder() )
	{
		if ( const auto* list = recorder->Get( mapName ) )
		{
			items.reserve( list->size() );
			for ( std::size_t i = 0; i < list->size(); ++i )
			{
				const auto& lu = ( *list )[ i ];
				if ( kindFilter != 0xff && lu.kind != kindFilter )
					continue;

				// 隐藏/覆盖标记放开头,与 Builtin 列表风格一致
				std::string label;
				if ( lu.hidden )
					label += "[hidden] ";
				label += std::to_string( i + 1 ) + ". " + lu.name;
				label += " (" + KindLabel( lu.kind ) + ")";
				items.push_back( std::move( label ) );
			}
		}
	}
	return items;
}

bool CHelper::RemoveRecorderItem( int index )
{
	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() || index < 0 )
		return false;
	if ( auto* recorder = GetHelperRecorder() )
		return recorder->Remove( mapName , static_cast<std::size_t>( index ) );
	return false;
}

void CHelper::ClearRecorderMap()
{
	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() )
		return;
	if ( auto* recorder = GetHelperRecorder() )
		recorder->ClearMap( mapName );
}

bool CHelper::GetRecorderItem( int index , UserLineup& out ) const
{
	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() || index < 0 )
		return false;
	if ( auto* recorder = GetHelperRecorder() )
	{
		if ( const auto* list = recorder->Get( mapName ) )
		{
			if ( static_cast<std::size_t>( index ) < list->size() )
			{
				out = ( *list )[ static_cast<std::size_t>( index ) ];
				return true;
			}
		}
	}
	return false;
}

bool CHelper::UpdateRecorderItem( int index , const UserLineup& lineup )
{
	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() || index < 0 )
		return false;
	if ( auto* recorder = GetHelperRecorder() )
		return recorder->Update( mapName , static_cast<std::size_t>( index ) , lineup );
	return false;
}

int CHelper::GetRecorderIndexAt( int listPos , std::uint8_t kindFilter ) const
{
	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() || listPos < 0 )
		return -1;

	if ( auto* recorder = GetHelperRecorder() )
	{
		if ( const auto* list = recorder->Get( mapName ) )
		{
			int matched = 0;
			for ( std::size_t i = 0; i < list->size(); ++i )
			{
				const auto& lu = ( *list )[ i ];
				if ( kindFilter != 0xff && lu.kind != kindFilter )
					continue;
				if ( matched == listPos )
					return static_cast<int>( i );
				++matched;
			}
		}
	}
	return -1;
}

// ============================================================================
// 菜单点位表:内置点位库(只读:列表/传送;参数化编辑不适用于逐帧数据)
// ============================================================================
std::vector<std::string> CHelper::BuildBuiltinItems( std::uint8_t kindFilter ) const
{
	std::vector<std::string> items;
	if ( !helper_timeline::Ready() )
		return items;

	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() )
		return items;

	const auto* points = helper_timeline::GetMapPoints( mapName );
	if ( !points )
		return items;

	int index = 0;
	for ( const auto& point : *points )
	{
		if ( kindFilter != 0xff && point.kind != kindFilter )
			continue;

		std::string label = "[" + std::to_string( index ) + "]";
		if ( point.hidden )
			label += " [hidden]";
		label += " " + point.name;
		label += " (" + KindLabel( point.kind ) + ")";
		items.push_back( std::move( label ) );
		++index;
	}
	return items;
}

// 时间线点位以"可传送"形式读出;Save/Remove 对时间线条目为 no-op
bool CHelper::GetBuiltinItem( int listPos , std::uint8_t kindFilter , UserLineup& out ) const
{
	if ( !helper_timeline::Ready() )
		return false;

	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() || listPos < 0 )
		return false;

	const auto* points = helper_timeline::GetMapPoints( mapName );
	if ( !points )
		return false;

	int matched = 0;
	for ( const auto& point : *points )
	{
		if ( kindFilter != 0xff && point.kind != kindFilter )
			continue;

		if ( matched == listPos )
		{
			const QAngle aimAngles = !point.frames.empty() ? point.frames.front().angles : point.angles;
			out = UserLineup{};
			out.name = point.name;
			out.weapon = point.weapon;
			out.x = point.position.m_x;
			out.y = point.position.m_y;
			out.z = point.position.m_z;
			out.pitch = aimAngles.m_x;
			out.yaw = aimAngles.m_y;
			out.kind = point.kind;
			out.hidden = point.hidden;
			out.builtin_id = point.id; // Save(隐藏)与编辑缓冲的 original 用它定位
			return true;
		}
		++matched;
	}
	return false;
}

// 内置时间线点位:仅支持隐藏覆盖(helper_lineups.dat 里写一条 builtin_id 条目)
bool CHelper::SaveBuiltinOverride( int builtinIndex , const UserLineup& lineup )
{
	auto* recorder = GetHelperRecorder();
	if ( !recorder || builtinIndex < 0 )
		return false;

	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() )
		return false;

	auto* point = helper_timeline::FindPointById( builtinIndex );
	if ( !point )
		return false;

	// 已有同 id 的覆盖条目 → 更新;无则追加(保留其它字段以便取消隐藏时还原)
	const auto* existing = recorder->Get( mapName );
	if ( !existing )
	{
		UserLineup entry;
		entry.builtin_id = builtinIndex;
		entry.hidden = lineup.hidden;
		entry.kind = point->kind;
		entry.name = point->name;
		recorder->Add( mapName , entry );
		point->hidden = lineup.hidden;
		return true;
	}
	auto& list = const_cast<std::vector<UserLineup>&>( *existing );
	for ( auto& lu : list )
	{
		if ( lu.builtin_id == builtinIndex )
		{
			lu.hidden = lineup.hidden;
			lu.name = point->name;
			recorder->Save();
			point->hidden = lineup.hidden;
			return true;
		}
	}

	// 列表存在但没有该 id 的覆盖条目 → 追加
	UserLineup entry;
	entry.builtin_id = builtinIndex;
	entry.hidden = lineup.hidden;
	entry.kind = point->kind;
	entry.name = point->name;
	recorder->Add( mapName , entry );
	point->hidden = lineup.hidden;
	return true;
}

bool CHelper::RemoveBuiltinOverride( int builtinIndex )
{
	auto* recorder = GetHelperRecorder();
	if ( !recorder || builtinIndex < 0 )
		return false;

	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() )
		return false;

	if ( const auto* list = recorder->Get( mapName ) )
	{
		for ( std::size_t i = 0; i < list->size(); ++i )
		{
			if ( ( *list )[ i ].builtin_id == builtinIndex )
			{
				recorder->Remove( mapName , i );
				if ( auto* point = helper_timeline::FindPointById( builtinIndex ) )
					point->hidden = false;
				return true;
			}
		}
	}
	return false;
}

// ============================================================================
// 录制会话(toggle 键:按下开始,再按一下保存)
// 时间线录制:就绪(手持武器且静止)后每 usercmd 采样一帧
// {按钮, 视角, 位置},出手(攻击松开)后再录 32 tick 尾巴即完成 —— 无参数推断。
// ============================================================================
void CHelper::BeginRecordSession()
{
	m_RecordSessionActive = true;
	m_SessionReady = false;
	m_SessionFrames.clear();
	m_SessionSawAttack = false;
	m_SessionTail = -1;
	m_SessionTailDone = false;
	m_SessionKind = 0xff;
	m_SessionArmPos = {};
	m_SessionThrowAngles = {};
	m_SessionStartTime = Now();
}

void CHelper::UpdateRecordSession()
{
	auto* player = GetCL_Players()->GetLocalPlayerPawn();
	if ( !player || !player->IsAlive() || player->m_bIsBuyMenuOpen() )
		return;

	const std::uint32_t tick = TickCount();

	// 就绪门槛:手持可用武器(雷或枪)且基本静止,避免把走过去/切雷录进起点
	if ( !m_SessionReady )
	{
		const std::uint8_t readyKind = ResolveWeaponKind();
		const Vector3 vel = player->m_vecAbsVelocity();
		const float horizSpeedSqr = vel.m_x * vel.m_x + vel.m_y * vel.m_y;
		const bool still = std::isfinite( vel.m_x ) && horizSpeedSqr <= ( 15.f * 15.f )
			&& std::abs( vel.m_z ) <= 12.f;
		if ( readyKind != 0xff && still )
		{
			m_SessionReady = true;
			m_SessionKind = readyKind;
			m_SessionArmPos = player->GetOrigin();
			GetRenderCameraAngles( m_SessionThrowAngles );
			m_SessionFrames.clear();
			m_SessionSawAttack = false;
			m_SessionTail = -1;
			m_SessionTailDone = false;
			m_SessionLastTick = tick;
		}
		return;
	}

	// 尾巴完成:等渲染侧落盘(与菜单同线程),不再采样
	if ( m_SessionTailDone )
		return;

	// 每 usercmd 一帧(tick 去重)
	if ( tick == m_SessionLastTick )
		return;
	m_SessionLastTick = tick;

	helper_timeline::Frame frame;
	if ( !GetRenderCameraAngles( frame.angles ) )
		frame.angles = m_SessionThrowAngles;
	frame.position = player->GetOrigin();

	// 按钮位:从本 command 的 button_states 读受管 10 位(纯读)
	if ( m_pCmd )
	{
		const std::uint64_t buttons = m_pCmd->button_states.buttonstate1;
		frame.in_attack    = ( buttons & IN_ATTACK ) != 0;
		frame.in_attack2   = ( buttons & IN_ATTACK2 ) != 0;
		frame.in_jump      = ( buttons & IN_JUMP ) != 0;
		frame.in_duck      = ( buttons & IN_DUCK ) != 0;
		frame.in_forward   = ( buttons & IN_FORWARD ) != 0;
		frame.in_back      = ( buttons & IN_BACK ) != 0;
		frame.in_use       = ( buttons & IN_USE ) != 0;
		frame.in_moveleft  = ( buttons & IN_MOVELEFT ) != 0;
		frame.in_moveright = ( buttons & IN_MOVERIGHT ) != 0;
		frame.in_speed     = ( buttons & IN_SPEED ) != 0;
	}

	m_SessionFrames.push_back( frame );

	if ( frame.in_attack || frame.in_attack2 )
		m_SessionSawAttack = true;

	// 雷类会话:攻击松开(左右键都松,雷离手)后录 32 tick 尾巴即完成
	const bool isNade = m_SessionKind != static_cast<std::uint8_t>( resources::nades::kind::wallbang );
	if ( isNade && m_SessionSawAttack && !frame.in_attack && !frame.in_attack2 && m_SessionTail < 0 )
	{
		m_SessionTail = 32;
	}

	if ( isNade && m_SessionTail > 0 )
	{
		--m_SessionTail;
		if ( m_SessionTail == 0 )
			m_SessionTailDone = true; // 渲染侧 Tick 负责落盘
	}
}

void CHelper::EndRecordSession()
{
	SaveSessionLineup();
	m_RecordSessionActive = false;
}

void CHelper::SaveSessionLineup()
{
	const std::string mapName = GetCurrentMapName();
	if ( mapName.empty() )
		return;

	auto* recorder = GetHelperRecorder();
	if ( !recorder )
		return;

	UserLineup lineup;
	lineup.kind = m_SessionKind;
	lineup.x = m_SessionArmPos.m_x;
	lineup.y = m_SessionArmPos.m_y;
	lineup.z = m_SessionArmPos.m_z;
	lineup.pitch = m_SessionThrowAngles.m_x;
	lineup.yaw = m_SessionThrowAngles.m_y;

	if ( m_SessionKind == static_cast<std::uint8_t>( resources::nades::kind::wallbang ) )
	{
		// 墙点:纯站位快照(无帧);默认勾选当前手持枪
		lineup.weapon = WeaponShortName( ActiveWeaponItem() );
		if ( lineup.name.empty() )
		{
			const auto* existing = recorder->Get( mapName );
			int wallCount = 1;
			if ( existing )
				for ( const auto& lu : *existing )
					if ( lu.kind == static_cast<std::uint8_t>( resources::nades::kind::wallbang ) )
						++wallCount;
			lineup.name = "Wall " + std::to_string( wallCount );
		}
	}
	else
	{
		// 雷类:时间线点位(要求真的出了手;没出手不保存)
		if ( m_SessionFrames.empty() || !m_SessionSawAttack || m_SessionTail < 0 )
		{
			DEV_LOG( "[helper] nade session discarded (no throw)" );
			return;
		}
		lineup.frames = m_SessionFrames;
		if ( lineup.name.empty() )
		{
			const auto* existing = recorder->Get( mapName );
			const int count = existing ? static_cast<int>( existing->size() ) : 0;
			lineup.name = "Custom " + std::to_string( count + 1 );
		}
	}

	const int index = recorder->Add( mapName , lineup );
	DEV_LOG( "[helper] saved %s on %s (idx=%d kind=%u frames=%zu)" ,
		lineup.name.c_str() , mapName.c_str() , index , lineup.kind , lineup.frames.size() );
}

// ============================================================================
// 录制状态卡片:屏幕底部中央的红色 REC + 已录制时长(仅录制会话中显示)
// ============================================================================
void CHelper::DrawRecordStatus( ImDrawList* drawList , int screenW , int screenH ) const
{
	if ( !m_RecordSessionActive )
		return;

	ImFont* font = ImGui::GetFont();
	if ( !font )
		return;

	// 已录制时长
	const auto now = Now();
	const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>( now - m_SessionStartTime ).count();
	char timeBuf[ 32 ];
	std::snprintf( timeBuf , sizeof( timeBuf ) , "%d.%02ds" ,
		static_cast<int>( elapsed / 1000 ) , static_cast<int>( ( elapsed % 1000 ) / 10 ) );

	// 呼吸红点(未就绪常亮偏暗,就绪后闪烁)
	const float pulse = 0.5f + 0.5f * std::sinf( static_cast<float>( ImGui::GetTime() ) * 6.f );
	const float dotAlpha = m_SessionReady ? 120.f + 135.f * pulse : 150.f;
	const ImU32 dotCol = IM_COL32( 255 , 40 , 40 , static_cast<int>( dotAlpha ) );

	// 标题行:● REC  3.25s
	const std::string label = "REC";
	const float titleSize = 18.f;
	const char* timeStr = timeBuf;

	// 参数行(仅就绪后):帧数 + 出手状态
	std::string params;
	if ( m_SessionReady )
	{
		char buf[ 160 ];
		std::snprintf( buf , sizeof( buf ) , "frames=%zu  %s" ,
			m_SessionFrames.size() ,
			m_SessionTail >= 0 ? "thrown" : "armed" );
		params = buf;
	}

	// 布局参数
	const float padX = 14.f;
	const float padY = 9.f;
	const float dotR = 5.f;
	const float gapTitle = 8.f;

	const ImVec2 titleSz = font->CalcTextSizeA( titleSize , FLT_MAX , -1.f , label.c_str() );
	const ImVec2 timeSz = font->CalcTextSizeA( titleSize , FLT_MAX , -1.f , timeStr );
	const float paramsSize = 14.f;
	const ImVec2 paramsSz = params.empty()
		? ImVec2{}
		: font->CalcTextSizeA( paramsSize , FLT_MAX , -1.f , params.c_str() );

	// 卡片尺寸:标题行(圆点+REC+时长)为最宽基准,参数行取较宽者
	const float titleW = dotR * 2.f + gapTitle + titleSz.x + gapTitle + timeSz.x;
	const float cardW = std::max( titleW , paramsSz.x ) + padX * 2.f;
	const float cardH = padY * 2.f + titleSz.y + ( params.empty() ? 0.f : paramsSize + 6.f );

	// 位置:底部偏上,水平居中
	const float bottomGap = 140.f;
	const ImVec2 cardPos( ( screenW - cardW ) * 0.5f , screenH - bottomGap - cardH );
	const ImVec2 cardEnd( cardPos.x + cardW , cardPos.y + cardH );

	// 半透明黑底圆角卡(无边框)
	drawList->AddRectFilled( cardPos , cardEnd , IM_COL32( 0 , 0 , 0 , 165 ) , 6.f );

	// 第一行:圆点 + REC + 时长(时长右对齐到卡片右缘)
	const float contentTop = cardPos.y + padY;
	const ImVec2 dotC( cardPos.x + padX + dotR , contentTop + titleSz.y * 0.5f );
	drawList->AddCircleFilled( dotC , dotR , dotCol );
	drawList->AddCircle( dotC , dotR , IM_COL32( 255 , 40 , 40 , 160 ) );

	const float textLeft = dotC.x + dotR + gapTitle;
	drawList->AddText( font , titleSize , { textLeft , contentTop } , IM_COL32( 255 , 90 , 90 , 255 ) , label.c_str() );

	const float timeLeft = cardEnd.x - padX - timeSz.x;
	drawList->AddText( font , titleSize , { timeLeft , contentTop } , IM_COL32( 245 , 247 , 252 , 255 ) , timeStr );

	// 第二行:实时参数(仅就绪后),浅灰白
	if ( !params.empty() )
	{
		const float paramsTop = contentTop + titleSz.y + 6.f;
		drawList->AddText( font , paramsSize , { cardPos.x + padX , paramsTop } ,
			IM_COL32( 210 , 215 , 225 , 235 ) , params.c_str() );
	}
}
