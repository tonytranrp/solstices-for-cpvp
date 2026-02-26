#include "NumberSettingScreen.hpp"

#include "SettingRenderContext.hpp"

#include <Features/GUI/ScreenManager/Core/ScreenManager.hpp>
#include <Features/Modules/Module.hpp>
#include <Features/Modules/Setting.hpp>
#include <Utils/MiscUtils/ImRenderUtils.hpp>
#include <Utils/MiscUtils/MathUtils.hpp>
#include <Utils/StringUtils.hpp>

#include <cmath>

namespace GuiScreen::Settings
{
    void renderNumberSetting(Setting* setting, NumberSetting* numberSetting, SettingRenderContext& context)
    {
        const float value = numberSetting->mValue;
        const float min = numberSetting->mMin;
        const float max = numberSetting->mMax;

        char str[10];
        sprintf_s(str, 10, "%.2f", value);
        std::string valueName = str;
        std::string setName = context.lowercase ? StringUtils::toLower(setting->mName) : setting->mName;

        context.moduleY = MathUtils::lerp(context.moduleY, context.moduleY + context.modHeight, context.module->cAnim);

        ImVec4 backGroundRect = ImVec4(
            context.modRect.x,
            context.categoryY + context.categoryHeaderHeight + context.moduleY,
            context.modRect.z,
            context.categoryY + context.categoryHeaderHeight + context.moduleY + context.modHeight)
            .scaleToPoint(ImVec4(context.modRect.x, context.screen.y / 2, context.modRect.z, context.screen.y / 2), context.inScale);
        backGroundRect.y = std::floor(backGroundRect.y);
        if (backGroundRect.y < context.modRect.y)
        {
            backGroundRect.y = context.modRect.y;
        }

        ImVec4 rect = ImVec4(
            context.modRect.x + 7.f,
            context.categoryY + context.categoryHeaderHeight + context.moduleY + context.setPadding,
            context.modRect.z - 7.f,
            context.categoryY + context.categoryHeaderHeight + context.moduleY + context.modHeight)
            .scaleToPoint(ImVec4(context.modRect.x, context.screen.y / 2, context.modRect.z, context.screen.y / 2), context.inScale);
        rect.y = std::floor(rect.y);
        if (rect.y < context.modRect.y)
        {
            rect.y = context.modRect.y;
        }

        static float clickAnimation = 1.f;
        if (ImGui::IsMouseDown(0) && ImRenderUtils::isMouseOver(rect))
        {
            clickAnimation = MathUtils::animate(0.60f, clickAnimation, ImRenderUtils::getDeltaTime() * 10.f);
        }
        else
        {
            clickAnimation = MathUtils::animate(1.f, clickAnimation, ImRenderUtils::getDeltaTime() * 10.f);
        }

        if (backGroundRect.y <= context.catRect.y + 0.5f)
        {
            return;
        }

        ImRenderUtils::fillRectangle(backGroundRect, ImColor(30, 30, 30), context.animation);
        const float sliderPos = (value - min) / (max - min) * (rect.z - rect.x);

        auto& numberState = context.manager.numberState(setting);
        numberState.sliderEase = MathUtils::animate(sliderPos, numberState.sliderEase, ImRenderUtils::getDeltaTime() * 10.f);
        numberState.sliderEase = std::clamp(numberState.sliderEase, 0.f, rect.getWidth());

        if (ImRenderUtils::isMouseOver(rect) && context.isEnabled && context.categoryExtended)
        {
            context.tooltip = setting->mDescription;
            if (ImGui::IsMouseDown(0) || ImGui::IsMouseDown(2))
            {
                numberState.isDragging = true;
                context.lastDraggedSetting = setting;
            }
        }

        if (ImGui::IsMouseDown(0) && numberState.isDragging && context.isEnabled)
        {
            if (context.lastDraggedSetting != setting)
            {
                numberState.isDragging = false;
            }
            else
            {
                const float newValue = std::fmax(
                    std::fmin((ImRenderUtils::getMousePos().x - rect.x) / (rect.z - rect.x) * (max - min) + min, max),
                    min);
                numberSetting->setValue(newValue);
            }
        }
        else if (ImGui::IsMouseDown(2) && numberState.isDragging && context.isEnabled)
        {
            if (context.lastDraggedSetting != setting)
            {
                numberState.isDragging = false;
            }
            else
            {
                float newValue = std::fmax(
                    std::fmin((ImRenderUtils::getMousePos().x - rect.x) / (rect.z - rect.x) * (max - min) + min, max),
                    min);
                newValue = std::round(newValue / context.midclickRounding) * context.midclickRounding;
                numberSetting->mValue = newValue;
            }
        }
        else
        {
            numberState.isDragging = false;
        }

        const float ySize = rect.w - rect.y;
        ImVec2 sliderBarMin = ImVec2(rect.x, rect.w - ySize / 8.f);
        ImVec2 sliderBarMax = ImVec2(rect.x + (numberState.sliderEase * context.inScale), rect.w);
        sliderBarMin.y = sliderBarMax.y - 4.f * context.inScale;
        ImVec4 sliderRect = ImVec4(sliderBarMin.x, sliderBarMin.y - 4.5f, sliderBarMax.x, sliderBarMax.y - 6.5f);

        ImRenderUtils::fillRectangle(sliderRect, context.themedColor, context.animation, 15);

        ImVec2 circlePos = ImVec2(sliderRect.z - 2.25f, sliderRect.getCenter().y);
        if (value <= min + 0.83f)
        {
            circlePos.x = sliderRect.z + 2.25f;
        }
        ImRenderUtils::fillCircle(circlePos, 5.5f * clickAnimation * context.animation, context.themedColor, context.animation, 12);

        ImGui::GetBackgroundDrawList()->PushClipRect(ImVec2(sliderRect.x, sliderRect.y), ImVec2(sliderRect.z, sliderRect.w), true);
        ImRenderUtils::fillShadowRectangle(sliderRect, context.themedColor, context.animation * 0.75f, 15.f, 0);
        ImGui::GetBackgroundDrawList()->PopClipRect();

        const auto valueLen = ImRenderUtils::getTextWidth(&valueName, context.textSize);
        ImRenderUtils::drawText(ImVec2((backGroundRect.z - 5.f) - valueLen, backGroundRect.y + 2.5f), valueName, ImColor(170, 170, 170), context.textSize, context.animation, true);
        ImRenderUtils::drawText(ImVec2(backGroundRect.x + 5.f, backGroundRect.y + 2.5f), setName, ImColor(255, 255, 255), context.textSize, context.animation, true);
    }
}

