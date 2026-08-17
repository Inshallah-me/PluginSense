#pragma once

#include "imgui.h"

struct ID3D11Device;

namespace fortnite_digits
{
    constexpr int kSourceWidth = 59;
    constexpr int kSourceHeight = 105;

    void Init(ID3D11Device* device) noexcept;
    bool Ready() noexcept;
    ImTextureID Get(int digit, int& outW, int& outH) noexcept;
}
