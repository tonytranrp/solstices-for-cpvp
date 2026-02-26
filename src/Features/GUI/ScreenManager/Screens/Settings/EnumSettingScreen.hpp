#pragma once

class Setting;
class EnumSetting;

namespace GuiScreen::Settings
{
    struct SettingRenderContext;

    void renderEnumSetting(Setting* setting, EnumSetting* enumSetting, SettingRenderContext& context);
}

