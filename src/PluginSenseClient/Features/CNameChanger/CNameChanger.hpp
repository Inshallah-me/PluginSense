#pragma once

#include <Common/Common.hpp>

class CNameChanger final
{
public:
	enum class Mode : int
	{
		Disabled = 0,
		Animated,      // preset clantag animation
		Static,        // custom static clantag text
		StaticRadar,   // custom static + radar toggle
		Minecraft,     // random characters
		CustomAnim,    // custom text + animation style
	};

	enum class AnimStyle : int
	{
		Static,
		RotationLR,
		RotationRL,
		Progressive,
		Retractable,
		RetractableFront,
		ScrollProg,
		PerTimePercent,
		Decode,
		Typewriter,
		Glitch,
		CoreDump,
		Penetrate,
		PasswordLock,
		ScanLine,
		Heart,
		CmdSpinner,
		CmdLog,
		CmdDots,
		NetError,
		DirBruteforce,
	};

public:
	auto Init() -> bool;
	auto Shutdown() -> void;
	auto OnFrame() -> void;
	auto ApplyName() -> void;
	auto RunCommand( const char* cmd ) -> void;

public:
	auto GetPresetCount() -> int;
	auto GetPresetName( int idx ) -> const char*;
	auto GetAnimStyleCount() -> int;
	auto GetAnimStyleName( int idx ) -> const char*;
};

auto GetNameChanger() -> CNameChanger*;
