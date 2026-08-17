#pragma once

#include <Common/Common.hpp>

class CCSPlayerInventory;

class CCSInventoryManager
{
public:
	static auto Get() -> CCSInventoryManager*;

public:
	// Vmt Index -> "59" -> "mov rax,qword ptr ds:[rcx+0x3D1A0]"
	auto GetLocalInventory() -> CCSPlayerInventory*;
};
