#pragma once

#include <imgui.h>

namespace GuiScreen
{
    struct ScreenContext
    {
        ImVec2 screenSize{};
        float animation = 0.f;
        float inScale = 1.f;
        bool isEnabled = false;
        bool lowercase = false;
    };
}

