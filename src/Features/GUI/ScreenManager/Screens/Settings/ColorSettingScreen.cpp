#include "ColorSettingScreen.hpp"

#include "SettingRenderContext.hpp"

#include <Features/GUI/ScreenManager/Core/ScreenManager.hpp>
#include <Features/Modules/Module.hpp>
#include <Features/Modules/Setting.hpp>
#include <Utils/MiscUtils/ImRenderUtils.hpp>
#include <Utils/MiscUtils/MathUtils.hpp>
#include <Utils/StringUtils.hpp>

namespace GuiScreen::Settings
{
    void renderColorSetting(Setting* setting, ColorSetting* colorSetting, SettingRenderContext& context)
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
            if (ImGui::IsMouseClicked(0) && !context.manager.isOverlayOpen() && context.module->showSettings)
            {
                context.manager.toggleColorPicker(colorSetting);
            }
        }

        const float cSetRectCentreY = rect.y + ((rect.w - rect.y) - context.textHeight) / 2;
        ImRenderUtils::drawText(ImVec2(rect.x + 5.f, cSetRectCentreY), setName, ImColor(255, 255, 255), context.textSize, context.animation, true);
        ImRenderUtils::fillRectangle(ImVec4(rect.z - 20.f, rect.y + 5.f, rect.z - 5.f, rect.w - 5.f), colorSetting->getAsImColor(), context.animation);
    }
}

