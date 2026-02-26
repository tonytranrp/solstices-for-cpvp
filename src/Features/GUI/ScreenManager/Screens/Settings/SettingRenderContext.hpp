#pragma once

#include <imgui.h>

#include <memory>
#include <string>

class Module;
class Setting;

namespace GuiScreen
{
    class ScreenManager;
}

namespace GuiScreen::Settings
{
    struct SettingRenderContext
    {
        ScreenManager& manager;
        std::shared_ptr<Module> module;
        float& moduleY;
        ImVec4 modRect;
        ImVec4 catRect;
        ImVec2 screen;
        float categoryY = 0.f;
        float categoryHeaderHeight = 0.f;
        float setPadding = 0.f;
        float modHeight = 0.f;
        float textHeight = 0.f;
        float textSize = 0.f;
        float inScale = 1.f;
        float animation = 1.f;
        float midclickRounding = 1.f;
        bool isEnabled = false;
        bool categoryExtended = false;
        bool lowercase = false;
        ImColor themedColor{};
        std::string& tooltip;
        Setting*& lastDraggedSetting;
    };
}
