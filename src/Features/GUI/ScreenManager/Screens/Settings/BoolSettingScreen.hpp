#pragma once

class Setting;
class BoolSetting;

namespace GuiScreen::Settings
{
    struct SettingRenderContext;

    void renderBoolSetting(Setting* setting, BoolSetting* boolSetting, SettingRenderContext& context);
}

