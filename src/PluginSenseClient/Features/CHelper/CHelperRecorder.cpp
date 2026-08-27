#include "CHelperRecorder.hpp"

#include <DllLauncher.hpp>
#include <Common/DevLog.hpp>

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

	std::string ReadStr( const rapidjson::Value& obj , const char* name )
	{
		if ( obj.HasMember( name ) && obj[ name ].IsString() )
			return obj[ name ].GetString();
		return {};
	}
}

static CHelperRecorder g_CHelperRecorder{};

std::string CHelperRecorder::BuildActionLabel( std::uint16_t actions )
{
	// 对齐内置点位风格:Crouch+ / Walk+ / Run+ / Jump+ + Throw
	std::string label;
	if ( actions & resources::nades::action_crouch )
		label += "Crouch+";
	if ( actions & resources::nades::action_run )
		label += ( actions & resources::nades::action_walk ) ? "Walk+" : "Run+";
	if ( actions & resources::nades::action_jump )
		label += "Jump+";
	label += "Throw";
	return label;
}

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
			lineup.action = ReadStr( entry , "action" );
			lineup.x = ReadFloat( entry , "x" , 0.f );
			lineup.y = ReadFloat( entry , "y" , 0.f );
			lineup.z = ReadFloat( entry , "z" , 0.f );
			lineup.pitch = ReadFloat( entry , "pitch" , 0.f );
			lineup.yaw = ReadFloat( entry , "yaw" , 0.f );
			lineup.kind = static_cast<std::uint8_t>( ReadInt( entry , "kind" , 0 ) );
			lineup.actions = static_cast<std::uint16_t>( ReadInt( entry , "actions" , 0 ) );
			lineup.run_ticks = static_cast<std::uint16_t>( ReadInt( entry , "run_ticks" , 0 ) );
			lineup.after_jump_ticks = static_cast<std::uint8_t>( ReadInt( entry , "after_jump_ticks" , 0 ) );
			lineup.throw_strength = ReadFloat( entry , "throw_strength" , 1.f );
			lineup.manual = entry.HasMember( "manual" ) && entry[ "manual" ].IsBool()
				? entry[ "manual" ].GetBool() : false;
			lineup.override_builtin_index = ReadInt( entry , "override_builtin_index" , -1 );
			lineup.hidden = entry.HasMember( "hidden" ) && entry[ "hidden" ].IsBool()
				? entry[ "hidden" ].GetBool() : false;

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
			writer.String( "name" );             writer.String( lineup.name.c_str() );
			writer.String( "action" );           writer.String( lineup.action.c_str() );
			writer.String( "x" );                writer.Double( lineup.x );
			writer.String( "y" );                writer.Double( lineup.y );
			writer.String( "z" );                writer.Double( lineup.z );
			writer.String( "pitch" );            writer.Double( lineup.pitch );
			writer.String( "yaw" );              writer.Double( lineup.yaw );
			writer.String( "kind" );             writer.Int( lineup.kind );
			writer.String( "actions" );          writer.Int( lineup.actions );
			writer.String( "run_ticks" );        writer.Int( lineup.run_ticks );
			writer.String( "after_jump_ticks" ); writer.Int( lineup.after_jump_ticks );
			writer.String( "throw_strength" );   writer.Double( lineup.throw_strength );
			writer.String( "manual" );           writer.Bool( lineup.manual );
			writer.String( "override_builtin_index" ); writer.Int( lineup.override_builtin_index );
			writer.String( "hidden" );           writer.Bool( lineup.hidden );
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