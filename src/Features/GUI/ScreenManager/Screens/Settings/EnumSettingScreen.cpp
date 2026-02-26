#include "EnumSettingScreen.hpp"

#include "SettingRenderContext.hpp"

#include <Features/GUI/ScreenManager/Core/ScreenManager.hpp>
#include <Features/Modules/Module.hpp>
#include <Features/Modules/Setting.hpp>
#include <Utils/MiscUtils/ImRenderUtils.hpp>
#include <Utils/MiscUtils/MathUtils.hpp>
#include <Utils/StringUtils.hpp>

namespace GuiScreen::Settings
{
    void renderEnumSetting(Setting* setting, EnumSetting* enumSetting, SettingRenderContext& context)
    {
        std::string setName = context.lowercase ? StringUtils::toLower(setting->mName) : setting->mName;
        std::vector<std::string> enumValues = enumSetting->mValues;
        if (context.lowercase)
        {
            for (std::string& value : enumValues)
            {
                value = StringUtils::toLower(value);
            }
        }

        int* iterator = &enumSetting->mValue;
        const int numValues = static_cast<int>(enumValues.size());
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

        auto& enumState = context.manager.enumState(setting);
        const float targetAnim = (enumState.extended && context.module->showSettings) ? 1.f : 0.f;
        enumState.slide = MathUtils::animate(targetAnim, enumState.slide, ImRenderUtils::getDeltaTime() * 10.f);
        enumState.slide = MathUtils::clamp(enumState.slide, 0.f, 1.f);

        if (enumState.slide > 0.001f)
        {
            for (int j = 0; j < numValues; j++)
            {
                std::string enumValue = enumValues[j];
                context.moduleY = MathUtils::lerp(context.moduleY, context.moduleY + context.modHeight, enumState.slide);

                ImVec4 rect2 = ImVec4(
                    context.modRect.x,
                    context.categoryY + context.categoryHeaderHeight + context.moduleY + context.setPadding,
                    context.modRect.z,
                    context.categoryY + context.categoryHeaderHeight + context.moduleY + context.modHeight)
                    .scaleToPoint(ImVec4(context.modRect.x, context.screen.y / 2, context.modRect.z, context.screen.y / 2), context.inScale);

                if (rect2.y <= context.catRect.y + 0.5f)
                {
                    continue;
                }

                const float cSetRectCentreY = rect2.y + ((rect2.w - rect2.y) - context.textHeight) / 2;
                ImRenderUtils::fillRectangle(rect2, ImColor(20, 20, 20), context.animation);

                if (*iterator == j)
                {
                    ImRenderUtils::fillRectangle(ImVec4(rect2.x, rect2.y, rect2.x + 1.5f, rect2.w), context.themedColor, context.animation);
                }

                if (ImRenderUtils::isMouseOver(rect2) &&
                    ImGui::IsMouseClicked(0) &&
                    context.isEnabled &&
                    !context.manager.isOverlayOpen() &&
                    context.module->showSettings)
                {
                    *iterator = j;
                }

                ImRenderUtils::drawText(ImVec2(rect2.x + 5.f, cSetRectCentreY), enumValue, ImColor(255, 255, 255), context.textSize, context.animation, true);
            }
        }

        if (rect.y > context.catRect.y + 0.5f)
        {
            ImRenderUtils::fillRectangle(rect, ImColor(30, 30, 30), context.animation);
            if (ImRenderUtils::isMouseOver(rect) && context.isEnabled && context.categoryExtended)
            {
                context.tooltip = setting->mDescription;
                if (ImGui::IsMouseClicked(0) && !context.manager.isOverlayOpen() && context.module->showSettings)
                {
                    *iterator = (*iterator + 1) % enumValues.size();
                }
                else if (ImGui::IsMouseClicked(1) && context.module->showSettings && !context.manager.isOverlayOpen())
                {
                    enumState.extended = !enumState.extended;
                }
            }

            const float cSetRectCentreY = rect.y + ((rect.w - rect.y) - context.textHeight) / 2;
            std::string settingString = enumValues[*iterator];
            const auto valueLen = ImRenderUtils::getTextWidth(&settingString, context.textSize);

            ImRenderUtils::drawText(ImVec2(rect.x + 5.f, cSetRectCentreY), setName, ImColor(255, 255, 255), context.textSize, context.animation, true);
            ImRenderUtils::drawText(
                ImVec2((rect.z - 5.f) - valueLen, cSetRectCentreY),
                settingString,
                ImColor(170, 170, 170),
                context.textSize,
                context.animation,
                true);
        }

        if (rect.y > context.catRect.y - context.modHeight)
        {
            ImRenderUtils::fillGradientOpaqueRectangle(
                ImVec4(rect.x, rect.w, rect.z, rect.w + 10.f * enumState.slide * context.animation),
                ImColor(0, 0, 0),
                ImColor(0, 0, 0),
                0.f * context.animation,
                0.55f * context.animation);
        }
    }
}

