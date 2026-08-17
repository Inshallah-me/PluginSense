#pragma once

#include <Windows.h>
#include <cstdint>

#include "Include/Config.hpp"
#include "Include/XorStr/XorStr.hpp"

#include "DevLog.hpp"

#if ENABLE_XOR_STR == 1

#define XorStr(str) xorstr_(str)

#else

#define XorStr(str) str

#endif

#define VmpStr(str) str

#define CS_CLASS_NO_ASSIGNMENT(CLASS)	\
	CLASS& operator=(CLASS&&) = delete;		\
	CLASS& operator=(const CLASS&) = delete;

#define IS_KEY_DOWN( KeyNum ) ( GetKeyState(KeyNum) & 0x100 )
