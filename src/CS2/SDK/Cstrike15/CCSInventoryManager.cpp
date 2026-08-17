#include "CCSInventoryManager.hpp"

#include <Common/MemoryEngine.hpp>

#include <CS2/SDK/FunctionListSDK.hpp>
#include <CS2/SDK/Update/Offsets.hpp>

auto CCSInventoryManager::Get() ->CCSInventoryManager*
{
	return CCSInventoryManager_Get();
}

auto CCSInventoryManager::GetLocalInventory() -> CCSPlayerInventory*
{
	return CUSTOM_OFFSET( CCSPlayerInventory* , g_CCSInventoryManager_GetLocalInventory );
}
