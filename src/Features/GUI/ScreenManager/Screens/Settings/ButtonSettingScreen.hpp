#pragma once

class Setting;
class ButtonSetting;

namespace GuiScreen::Settings
{
    struct SettingRenderContext;

    void renderButtonSetting(Setting* setting, ButtonSetting* buttonSetting, SettingRenderContext& context);
}
