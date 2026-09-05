#include "CHelperRecorder.hpp"

#include <DllLauncher.hpp>
#include <Common/DevLog.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>

#include <rapidjson/document.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/istreamwrapper.h>
#include <rapidjson/ostreamwrapper.h>
#include <rapidjson/error/en.h>

namespace
{
	// 用 .dat 扩展名:避免被配置面板的 .json 扫描当成配置文件
	constexpr const char* kLineupFile = "helper_lineups.dat";

	// 帧按钮掩码位(与 gen_timeline_data.py / HelperTimelineData.hpp 一致)
	enum : std::uint16_t
	{
		rbtn_attack    = 1 << 0,
		rbtn_jump      = 1 << 1,
		rbtn_duck      = 1 << 2,
		rbtn_forward   = 1 << 3,
		rbtn_back      = 1 << 4,
		rbtn_use       = 1 << 5,
		rbtn_moveleft  = 1 << 6,
		rbtn_moveright = 1 << 7,
		rbtn_attack2   = 1 << 8,
		rbtn_speed     = 1 << 9,
	};

	std::uint16_t EncodeFrameButtons( const helper_timeline::Frame& f )
	{
		std::uint16_t mask = 0;
		if ( f.in_attack )    mask |= rbtn_attack;
		if ( f.in_jump )      mask |= rbtn_jump;
		if ( f.in_duck )      mask |= rbtn_duck;
		if ( f.in_forward )   mask |= rbtn_forward;
		if ( f.in_back )      mask |= rbtn_back;
		if ( f.in_use )       mask |= rbtn_use;
		if ( f.in_moveleft )  mask |= rbtn_moveleft;
		if ( f.in_moveright ) mask |= rbtn_moveright;
		if ( f.in_attack2 )   mask |= rbtn_attack2;
		if ( f.in_speed )     mask |= rbtn_speed;
		return mask;
	}

	void DecodeFrameButtons( std::uint16_t mask , helper_timeline::Frame& f )
	{
		f.in_attack    = ( mask & rbtn_attack ) != 0;
		f.in_jump      = ( mask & rbtn_jump ) != 0;
		f.in_duck      = ( mask & rbtn_duck ) != 0;
		f.in_forward   = ( mask & rbtn_forward ) != 0;
		f.in_back      = ( mask & rbtn_back ) != 0;
		f.in_use       = ( mask & rbtn_use ) != 0;
		f.in_moveleft  = ( mask & rbtn_moveleft ) != 0;
		f.in_moveright = ( mask & rbtn_moveright ) != 0;
		f.in_attack2   = ( mask & rbtn_attack2 ) != 0;
		f.in_speed     = ( mask & rbtn_speed ) != 0;
	}

	std::string LineupFilePath()
	{
		return GetConfigDir() + kLineupFile;
	}

	// 从 json 对象安全读一个字段(缺失/类型不符则保留默认值)
	float ReadFloat( const rapidjson::Value& obj , const char* name , float fallback )
	{
		if ( obj.HasMember( name ) && obj[ name ].IsNumber() )
			return obj[ name ].GetFloat();
		return fallback;
	}

	int ReadInt( const rapidjson::Value& obj , const char* name , int fallback )
	{
		if ( obj.HasMember( name ) && obj[ name ].IsInt() )
			return obj[ name ].GetInt();
		return fallback;
	}

	bool ReadBool( const rapidjson::Value& obj , const char* name , bool fallback )
	{
		if ( obj.HasMember( name ) && obj[ name ].IsBool() )
			return obj[ name ].GetBool();
		return fallback;
	}

	std::string ReadStr( const rapidjson::Value& obj , const char* name )
	{
		if ( obj.HasMember( name ) && obj[ name ].IsString() )
			return obj[ name ].GetString();
		return {};
	}
}

static CHelperRecorder g_CHelperRecorder{};

int CHelperRecorder::Add( const std::string& mapName , const UserLineup& lineup )
{
	if ( mapName.empty() )
		return -1;

	auto& list = m_Maps[ mapName ];
	list.push_back( lineup );
	const int index = static_cast<int>( list.size() ) - 1;
	Save();
	return index;
}

bool CHelperRecorder::Update( const std::string& mapName , std::size_t index , const UserLineup& lineup )
{
	const auto it = m_Maps.find( mapName );
	if ( it == m_Maps.end() || index >= it->second.size() )
		return false;

	it->second[ index ] = lineup;
	Save();
	return true;
}

bool CHelperRecorder::Remove( const std::string& mapName , std::size_t index )
{
	const auto it = m_Maps.find( mapName );
	if ( it == m_Maps.end() || index >= it->second.size() )
		return false;

	it->second.erase( it->second.begin() + static_cast<std::ptrdiff_t>( index ) );
	if ( it->second.empty() )
		m_Maps.erase( it );
	Save();
	return true;
}

void CHelperRecorder::ClearMap( const std::string& mapName )
{
	const auto it = m_Maps.find( mapName );
	if ( it == m_Maps.end() )
		return;

	m_Maps.erase( it );
	Save();
}

const std::vector<UserLineup>* CHelperRecorder::Get( const std::string& mapName ) const
{
	const auto it = m_Maps.find( mapName );
	if ( it == m_Maps.end() || it->second.empty() )
		return nullptr;
	return &it->second;
}

void CHelperRecorder::Load()
{
	m_Maps.clear();

	std::ifstream file( LineupFilePath() );
	if ( !file.is_open() )
		return;

	rapidjson::IStreamWrapper wrapper( file );
	rapidjson::Document doc;
	doc.ParseStream( wrapper );

	if ( doc.HasParseError() )
	{
		DEV_LOG( "[helper] lineup load parse error: %s @ %zu" ,
			rapidjson::GetParseError_En( doc.GetParseError() ) , doc.GetErrorOffset() );
		return;
	}
	if ( !doc.IsObject() || !doc.HasMember( "maps" ) || !doc[ "maps" ].IsObject() )
		return;

	const auto& maps = doc[ "maps" ];
	for ( auto mapIt = maps.MemberBegin(); mapIt != maps.MemberEnd(); ++mapIt )
	{
		if ( !mapIt->value.IsArray() )
			continue;

		const std::string mapName = mapIt->name.GetString();
		auto& list = m_Maps[ mapName ];

		for ( const auto& entry : mapIt->value.GetArray() )
		{
			if ( !entry.IsObject() )
				continue;

			UserLineup lineup;
			lineup.name = ReadStr( entry , "name" );
			lineup.weapon = ReadStr( entry , "weapon" );
			lineup.x = ReadFloat( entry , "x" , 0.f );
			lineup.y = ReadFloat( entry , "y" , 0.f );
			lineup.z = ReadFloat( entry , "z" , 0.f );
			lineup.pitch = ReadFloat( entry , "pitch" , 0.f );
			lineup.yaw = ReadFloat( entry , "yaw" , 0.f );
			lineup.kind = static_cast<std::uint8_t>( ReadInt( entry , "kind" , 0 ) );
			lineup.hidden = ReadBool( entry , "hidden" , false );
			lineup.builtin_id = ReadInt( entry , "builtin_id" , -1 );
			lineup.annotations = static_cast<std::uint8_t>( ReadInt( entry , "annotations" , 0 ) );

			// 时间线帧(雷类条目);墙点条目没有 frames
			if ( entry.HasMember( "frames" ) && entry[ "frames" ].IsArray() )
			{
				for ( const auto& fr : entry[ "frames" ].GetArray() )
				{
					if ( !fr.IsObject() )
						continue;

					helper_timeline::Frame frame;
					DecodeFrameButtons( static_cast<std::uint16_t>( ReadInt( fr , "b" , 0 ) ) , frame );
					frame.angles = QAngle(
						ReadFloat( fr , "pitch" , 0.f ) ,
						ReadFloat( fr , "yaw" , 0.f ) , 0.f );
					frame.position = Vector3(
						ReadFloat( fr , "x" , 0.f ) ,
						ReadFloat( fr , "y" , 0.f ) ,
						ReadFloat( fr , "z" , 0.f ) );

					lineup.frames.push_back( std::move( frame ) );
				}
			}

			// 隐藏覆盖条目(builtin_id >= 0,无帧)原样保留
			// 旧参数格式的雷类条目(无 frames)按约定丢弃;墙点快照保留
			if ( lineup.builtin_id < 0
				&& lineup.kind != static_cast<std::uint8_t>( resources::nades::kind::wallbang )
				&& lineup.frames.empty() )
				continue;

			list.push_back( std::move( lineup ) );
		}

		if ( list.empty() )
			m_Maps.erase( mapName );
	}
}

void CHelperRecorder::Save() const
{
	std::filesystem::create_directories( GetConfigDir() );

	std::ofstream file( LineupFilePath() );
	if ( !file.is_open() )
	{
		DEV_LOG( "[helper] lineup save: cannot open %s" , LineupFilePath().c_str() );
		return;
	}

	rapidjson::OStreamWrapper wrapper( file );
	rapidjson::PrettyWriter<rapidjson::OStreamWrapper> writer( wrapper );
	writer.SetIndent( '\t' , 1 );
	writer.SetMaxDecimalPlaces( 4 );

	writer.StartObject();
	writer.String( "maps" );
	writer.StartObject();
	for ( const auto& [ mapName , list ] : m_Maps )
	{
		if ( list.empty() )
			continue;

		writer.String( mapName.c_str() );
		writer.StartArray();
		for ( const auto& lineup : list )
		{
			writer.StartObject();
			writer.String( "name" );   writer.String( lineup.name.c_str() );
			writer.String( "weapon" ); writer.String( lineup.weapon.c_str() );
			writer.String( "kind" );   writer.Int( lineup.kind );
			writer.String( "x" );      writer.Double( lineup.x );
			writer.String( "y" );      writer.Double( lineup.y );
			writer.String( "z" );      writer.Double( lineup.z );
			writer.String( "pitch" );  writer.Double( lineup.pitch );
			writer.String( "yaw" );    writer.Double( lineup.yaw );
			writer.String( "hidden" ); writer.Bool( lineup.hidden );
			writer.String( "builtin_id" ); writer.Int( lineup.builtin_id );
			writer.String( "annotations" ); writer.Int( lineup.annotations );

			if ( !lineup.frames.empty() )
			{
				writer.String( "frames" );
				writer.StartArray();
				for ( const auto& frame : lineup.frames )
				{
					writer.StartObject();
					writer.String( "b" );     writer.Int( EncodeFrameButtons( frame ) );
					writer.String( "pitch" ); writer.Double( frame.angles.m_x );
					writer.String( "yaw" );   writer.Double( frame.angles.m_y );
					writer.String( "x" );     writer.Double( frame.position.m_x );
					writer.String( "y" );     writer.Double( frame.position.m_y );
					writer.String( "z" );     writer.Double( frame.position.m_z );
					writer.EndObject();
				}
				writer.EndArray();
			}
			writer.EndObject();
		}
		writer.EndArray();
	}
	writer.EndObject();
	writer.EndObject();
}

auto GetHelperRecorder() -> CHelperRecorder*
{
	return &g_CHelperRecorder;
}
