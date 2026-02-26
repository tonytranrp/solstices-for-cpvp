#pragma once

class Setting;
class ListSetting;

namespace GuiScreen::Settings
{
    struct SettingRenderContext;

    void renderListSetting(Setting* setting, ListSetting* listSetting, SettingRenderContext& context);
}

