#pragma once

namespace GuiScreen
{
    class ScreenManager;
    struct ScreenContext;
}

namespace GuiScreen::Screens
{
    void renderColorPickerScreen(ScreenManager& manager, const ScreenContext& context);
}

