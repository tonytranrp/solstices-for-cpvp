#include "ColorPickerScreen.hpp"

#include <Features/GUI/ScreenManager/Core/ScreenContext.hpp>
#include <Features/GUI/ScreenManager/Core/ScreenManager.hpp>
#include <Features/Modules/Setting.hpp>
#include <Utils/FontHelper.hpp>
#include <Utils/MiscUtils/ImRenderUtils.hpp>

namespace GuiScreen::Screens
{
    void renderColorPickerScreen(ScreenManager& manager, const ScreenContext& context)
    {
        if (!context.isEnabled)
        {
            manager.closeColorPicker();
            return;
        }

        if (!manager.isColorPickerOpen())
        {
            return;
        }

        ColorSetting* colorSetting = manager.activeColorSetting();
        if (colorSetting == nullptr)
        {
            manager.closeColorPicker();
            return;
        }

        FontHelper::pushPrefFont(false, false, true);
        ImGui::SetNextWindowPos(ImVec2(context.screenSize.x / 2 - 200.f, context.screenSize.y / 2), ImGuiCond_Always);
        ImGui::SetNextWindowSize(ImVec2(400.f, 400.f), ImGuiCond_Always);

        ImGui::Begin("Color Picker", nullptr, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
        {
            const ImVec4 currentColor = colorSetting->getAsImColor().Value;
            ImGui::ColorPicker4("Color", colorSetting->mValue, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_NoAlpha);
            ImGui::Button("Close");
            if (ImGui::IsItemClicked())
            {
                colorSetting->setFromImColor(ImColor(currentColor));
                manager.closeColorPicker();
            }
        }
        ImGui::End();
        ImGui::PopFont();

        if (ImGui::IsMouseClicked(0) &&
            !ImRenderUtils::isMouseOver(ImVec4(
                context.screenSize.x / 2 - 200.f,
                context.screenSize.y / 2,
                context.screenSize.x / 2 + 200.f,
                context.screenSize.y / 2 + 400.f)))
        {
            manager.closeColorPicker();
        }
    }
}
