#include "ButtonSettingScreen.hpp"

#include "SettingRenderContext.hpp"

#include <Features/GUI/ScreenManager/Core/ScreenManager.hpp>
#include <Features/Modules/Module.hpp>
#include <Features/Modules/Setting.hpp>
#include <Utils/MiscUtils/ImRenderUtils.hpp>
#include <Utils/MiscUtils/MathUtils.hpp>
#include <Utils/StringUtils.hpp>

namespace GuiScreen::Settings
{
    void renderButtonSetting(Setting* setting, ButtonSetting* buttonSetting, SettingRenderContext& context)
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

        const float cSetRectCentreY = rect.y + ((rect.w - rect.y) - context.textHeight) / 2.f;
        ImRenderUtils::drawText(
            ImVec2(rect.x + 5.f, cSetRectCentreY),
            setName,
            ImColor(255, 255, 255),
            context.textSize,
            context.animation,
            true);

        const bool canClick = buttonSetting->canClick();
        const ImVec4 actionRect(rect.z - 90.f, rect.y + 4.f, rect.z - 6.f, rect.w - 4.f);
        const bool actionHovered = hovered && canClick && ImRenderUtils::isMouseOver(actionRect);
        const ImColor buttonColor = canClick ? ImColor(22, 22, 22) : ImColor(18, 18, 18);
        const ImColor buttonTextColor = canClick ? ImColor(255, 255, 255) : ImColor(150, 150, 150);
        const float borderAlpha = canClick ? (actionHovered ? 0.95f : 0.45f) : 0.25f;

        ImRenderUtils::fillRectangle(actionRect, buttonColor, context.animation, 4.f);
        ImRenderUtils::drawRoundRect(actionRect, ImDrawFlags_RoundCornersAll, 4.f, context.themedColor, borderAlpha * context.animation, 1.f);

        std::string buttonText = buttonSetting->mButtonText;
        if (buttonText.empty())
        {
            buttonText = context.lowercase ? "run" : "Run";
        }
        else if (context.lowercase)
        {
            buttonText = StringUtils::toLower(buttonText);
        }

        const float buttonTextWidth = ImRenderUtils::getTextWidth(&buttonText, context.textSize * 0.9f);
        const float buttonTextY = actionRect.y + ((actionRect.w - actionRect.y) - context.textHeight) * 0.5f;
        ImRenderUtils::drawText(
            ImVec2(actionRect.x + ((actionRect.z - actionRect.x) - buttonTextWidth) * 0.5f, buttonTextY),
            buttonText,
            buttonTextColor,
            context.textSize * 0.9f,
            context.animation,
            true);

        if (actionHovered && ImGui::IsMouseClicked(0) && context.module->showSettings && !context.manager.isOverlayOpen())
        {
            buttonSetting->click();
        }
    }
}
