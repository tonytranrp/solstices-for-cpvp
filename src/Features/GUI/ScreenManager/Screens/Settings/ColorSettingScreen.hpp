#pragma once

class Setting;
class ColorSetting;

namespace GuiScreen::Settings
{
    struct SettingRenderContext;

    void renderColorSetting(Setting* setting, ColorSetting* colorSetting, SettingRenderContext& context);
}

