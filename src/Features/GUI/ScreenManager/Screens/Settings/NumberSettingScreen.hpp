#pragma once

class Setting;
class NumberSetting;

namespace GuiScreen::Settings
{
    struct SettingRenderContext;

    void renderNumberSetting(Setting* setting, NumberSetting* numberSetting, SettingRenderContext& context);
}

