#include "ListSettingScreen.hpp"

#include "SettingRenderContext.hpp"

#include <Features/GUI/ScreenManager/Core/ScreenManager.hpp>
#include <Features/Modules/Module.hpp>
#include <Features/Modules/Setting.hpp>
#include <Utils/MiscUtils/ImRenderUtils.hpp>
#include <Utils/MiscUtils/MathUtils.hpp>
#include <Utils/StringUtils.hpp>

namespace GuiScreen::Settings
{
    void renderListSetting(Setting* setting, ListSetting* listSetting, SettingRenderContext& context)
    {
        const std::string setName = context.lowercase ? StringUtils::toLower(setting->mName) : setting->mName;
        context.moduleY = MathUtils::lerp(context.moduleY, context.moduleY + context.modHeight, context.module->cAnim);

        ImVec4 rect = ImVec4(
            context.modRect.x,
            context.categoryY + context.categoryHeaderHeight + context.moduleY + context.setPadding,
            context.modRect.z,
            context.categoryY + context.categoryHeaderHeight + context.moduleY + context.modHeight)
            .scaleToPoint(ImVec4(context.modRect.x, context.screen.y / 2, context.modRect.z, context.screen.y / 2), context.inScale);
        rect.y = std::floor(rect.y);
        if (rect.y < context.modRect.y)
        {
            rect.y = context.modRect.y;
        }

        if (rect.y <= context.catRect.y + 0.5f)
        {
            return;
        }

        ImRenderUtils::fillRectangle(rect, ImColor(30, 30, 30), context.animation);
        const bool hovered = ImRenderUtils::isMouseOver(rect) && context.isEnabled && context.categoryExtended;
        if (hovered)
        {
            context.tooltip = setting->mDescription;
        }

        const float cSetRectCentreY = rect.y + ((rect.w - rect.y) - context.textHeight) / 2;
        ImRenderUtils::drawText(ImVec2(rect.x + 5.f, cSetRectCentreY), setName, ImColor(255, 255, 255), context.textSize, context.animation, true);

        std::string selectedText = std::to_string(listSetting->mSelectedValues.size()) + "/" + std::to_string(listSetting->mOptions.size());
        const float selectedTextWidth = ImRenderUtils::getTextWidth(&selectedText, context.textSize);
        ImRenderUtils::drawText(
            ImVec2(rect.z - 72.f - selectedTextWidth, cSetRectCentreY),
            selectedText,
            ImColor(170, 170, 170),
            context.textSize,
            context.animation,
            true);

        const ImVec4 chooseButtonRect(rect.z - 64.f, rect.y + 4.f, rect.z - 6.f, rect.w - 4.f);
        ImRenderUtils::fillRectangle(chooseButtonRect, ImColor(22, 22, 22), context.animation, 4.f);
        std::string chooseText = context.lowercase ? "choose" : "Choose";
        const float chooseTextWidth = ImRenderUtils::getTextWidth(&chooseText, context.textSize * 0.9f);
        const float chooseTextY = chooseButtonRect.y + ((chooseButtonRect.w - chooseButtonRect.y) - context.textHeight) * 0.5f;
        ImRenderUtils::drawText(
            ImVec2(chooseButtonRect.x + ((chooseButtonRect.z - chooseButtonRect.x) - chooseTextWidth) * 0.5f, chooseTextY),
            chooseText,
            ImColor(255, 255, 255),
            context.textSize * 0.9f,
            context.animation,
            true);

        const bool chooseHovered = hovered && ImRenderUtils::isMouseOver(chooseButtonRect);
        if ((hovered || chooseHovered) && ImGui::IsMouseClicked(0) && context.module->showSettings)
        {
            context.manager.closeColorPicker();
            context.manager.openListChooser(listSetting);
        }
        else if (hovered && ImGui::IsMouseClicked(1) && !context.manager.isOverlayOpen() && context.module->showSettings)
        {
            listSetting->clearSelection();
        }
    }
}
