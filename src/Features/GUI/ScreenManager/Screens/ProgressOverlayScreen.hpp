#pragma once

namespace GuiScreen
{
    class ScreenManager;
    struct ScreenContext;
}

namespace GuiScreen::Screens
{
    void renderProgressOverlayScreen(ScreenManager& manager, const ScreenContext& context);
}
