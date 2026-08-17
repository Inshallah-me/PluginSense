#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <cstdint>
#include <cmath>
#include <cfloat>
#include <algorithm>
#include <array>
#include <codecvt>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <functional>
#include <locale>
#include <map>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if __has_include(<imgui.h>)
#include <imgui.h>
#elif __has_include(<ImGui/imgui.h>)
#include <ImGui/imgui.h>
#endif

#if __has_include(<imgui_internal.h>)
#include <imgui_internal.h>
#elif __has_include(<ImGui/imgui_internal.h>)
#include <ImGui/imgui_internal.h>
#endif

#if __has_include(<misc/freetype/imgui_freetype.h>)
#include <misc/freetype/imgui_freetype.h>
#elif __has_include(<ImGui/Misc/freetype/imgui_freetype.h>)
#include <ImGui/Misc/freetype/imgui_freetype.h>
#endif

struct overlay_context_t {
    int width{ 1920 };
    int height{ 1080 };
    ID3D11Device* m_device{ nullptr };
    ID3D11DeviceContext* m_context{ nullptr };
    IDXGISwapChain* m_swap_chain{ nullptr };
};

namespace core {
    inline auto g_overlay = std::make_shared<overlay_context_t>();
}

namespace framework {
    inline auto& g_overlay = core::g_overlay;
}

namespace ImGuiDir_ {
    static constexpr ImGuiDir ImGuiDir_Left = ::ImGuiDir_Left;
    static constexpr ImGuiDir ImGuiDir_Right = ::ImGuiDir_Right;
    static constexpr ImGuiDir ImGuiDir_Up = ::ImGuiDir_Up;
    static constexpr ImGuiDir ImGuiDir_Down = ::ImGuiDir_Down;
}

namespace slog {
    struct log {
        template <typename... args_t> static void info(const char*, args_t&&...) {}
        template <typename... args_t> static void success(const char*, args_t&&...) {}
        template <typename... args_t> static void debug(const char*, args_t&&...) {}
        template <typename... args_t> static void warn(const char*, args_t&&...) {}
        template <typename... args_t> static void error(const char*, args_t&&...) {}
    };
}

namespace ImGuiFreeType {
    static inline bool BuildFontAtlas(ImFontAtlas* atlas, unsigned int flags = 0)
    {
        atlas->FontBuilderIO = GetBuilderForFreeType();
        atlas->FontBuilderFlags = flags;
        return atlas->Build();
    }
}

#include "math/math.hh"
#include "animations/animations.hh"
#include "framework/context/context.hh"
#include "render/render.hh"
#include "render/blur_engine/blur_engine.hh"
#include "framework/gui/base_element/base_element.hh"
#include "framework/gui/checkbox/checkbox.hh"
#include "framework/gui/slider/slider.hh"
#include "framework/gui/dropdown/dropdown.hh"
#include "framework/gui/multibox/multibox.hh"
#include "framework/gui/colorpicker/colorpicker.hh"
#include "framework/gui/keybind/keybind.hh"
#include "framework/gui/textinput/textinput.hh"
#include "framework/gui/button/button.hh"
#include "framework/gui/popup/popup.hh"
#include "framework/gui/listbox/listbox.hh"
#include "framework/gui/text/text.hh"
#include "framework/gui/search/search.hh"
#include "framework/gui/widgets/widgets.hh"
#include "framework/gui/interactive_preview/interactive_preview.hh"
#include "framework/gui/tab/tab.hh"
#include "framework/gui/child/child.hh"
#include "framework/gui/window/window.hh"

struct ash_config_t {
    bool enabled{ false };
    int particle_type{ 0 };
    int count{ 500 };
    float radius{ 500.f };
    float speed{ 1.f };
    float wind_x{ 0.f };
    float turbulence{ 1.f };
    float glow_intensity{ 1.f };
    hue::c_color ember_core{ 255, 120, 60, 255 };
    hue::c_color ember_glow{ 255, 80, 30, 255 };
    hue::c_color debris_color{ 120, 120, 120, 255 };
    hue::c_color snow_color{ 255, 255, 255, 255 };
    hue::c_color rain_color{ 120, 170, 255, 255 };
    hue::c_color star_color{ 255, 255, 220, 255 };
    hue::c_color star_glow{ 255, 220, 120, 255 };
};

struct dissolve_config_t {
    bool enabled{ false };
    int max_particles{ 64 };
    float spread_speed{ 1.f };
    float size_min{ 1.f };
    float size_max{ 4.f };
    float duration{ 1.f };
    float glow_intensity{ 1.f };
    hue::c_color color{ 255, 255, 255, 255 };
};

#include "framework/config_system/config_system.hh"
#include "framework/menu.hh"


