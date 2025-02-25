#pragma once
#include <vector>
#include <Features/FeatureManager.hpp>
#include <Features/Modules/Setting.hpp>
#include <Features/Modules/ModuleCategory.hpp>
#include <Features/Modules/Visual/ClickGui.hpp>
#include <Utils/FontHelper.hpp>
#include <Utils/MiscUtils/ImRenderUtils.hpp>
#include <Utils/MiscUtils/MathUtils.hpp>
#include <Features/Modules/Setting.hpp>
#include <Features/Modules/Visual/Interface.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Rendering/GuiData.hpp>
#include <Utils/Keyboard.hpp>
#include <Utils/StringUtils.hpp>
#include <Utils/MiscUtils/ColorUtils.hpp>

/**
 * @brief Renders a number (slider) setting UI component.
 *
 * This class encapsulates the slider rendering and dragging logic for a NumberSetting.
 */
class NumberSettingRenderer {
public:
    /**
     * @brief Renders the number (slider) setting.
     *
     * @param numSetting Pointer to the NumberSetting instance to render.
     * @param modRect The module rectangle.
     * @param catRect The current category rectangle.
     * @param baseY The base Y coordinate (typically catPositions[i].y + catHeight).
     * @param modHeight Height of one module row.
     * @param setPadding Vertical padding for the setting row.
     * @param animation Current animation factor.
     * @param inScale UI scale factor.
     * @param screenHalfY Half the screen height (e.g. screen.y / 2).
     * @param textHeight Height of the text.
     * @param textSize Text size.
     * @param isEnabled Whether the GUI is enabled.
     * @param lowercase Whether to convert the setting name to lowercase.
     * @param moduleCAnim The module’s current animation factor (e.g. mod->cAnim).
     * @param moduleY In/out: the current vertical offset for this module (will be updated).
     * @param midclickRounding Rounding factor used when dragging with mouse button 2.
     * @param showSettings Whether the module’s settings are visible.
     * @param isExtended Whether the current category is extended.
     * @param tooltip Out: will be set if the mouse hovers over the slider.
     * @param radius Corner radius for rounded drawing.
     * @param lastDraggedSetting In/out: a pointer to the last setting being dragged.
     */
    static void render(NumberSetting* numSetting,
        const ImVec4& modRect,
        const ImVec4& catRect,
        float baseY,
        float modHeight,
        float setPadding,
        float animation,
        float inScale,
        float screenHalfY,
        float textHeight,
        float textSize,
        bool isEnabled,
        bool lowercase,
        float moduleCAnim,
        float& moduleY,
        float midclickRounding,
        bool showSettings,
        bool isExtended,
        std::string& tooltip,
        float radius,
        Setting*& lastDraggedSetting)
    {
        // Retrieve the current value and bounds.
        const float value = numSetting->mValue;
        const float minVal = numSetting->mMin;
        const float maxVal = numSetting->mMax;

        // Convert the current value to a string.
        char buf[10];
        sprintf_s(buf, "%.2f", value);
        std::string valueStr(buf);

        // Get the setting name, applying lowercase if requested.
        std::string setName = lowercase ? StringUtils::toLower(numSetting->mName) : numSetting->mName;

        // Animate the vertical offset for this setting.
        moduleY = MathUtils::lerp(moduleY, moduleY + modHeight, moduleCAnim);

        // Compute the background rectangle for the slider.
        ImVec4 bgRect = ImVec4(
            modRect.x,
            baseY + moduleY,
            modRect.z,
            baseY + moduleY + modHeight
        ).scaleToPoint(ImVec4(modRect.x, screenHalfY, modRect.z, screenHalfY), inScale);
        bgRect.y = std::max(std::floor(bgRect.y), modRect.y);

        // Compute the slider area rectangle with horizontal padding.
        ImVec4 sliderArea = ImVec4(
            modRect.x + 7,
            baseY + moduleY + setPadding,
            modRect.z - 7,
            baseY + moduleY + modHeight
        ).scaleToPoint(ImVec4(modRect.x, screenHalfY, modRect.z, screenHalfY), inScale);
        sliderArea.y = std::max(std::floor(sliderArea.y), modRect.y);

        // Determine the slider color using a themed color (for example purposes).
        ImColor sliderColor = ColorUtils::getThemedColor(moduleY * 2);

        // Animate click feedback.
        static float clickAnim = 1.f;
        if (ImGui::IsMouseDown(0) && ImRenderUtils::isMouseOver(sliderArea))
            clickAnim = MathUtils::animate(0.60f, clickAnim, ImRenderUtils::getDeltaTime() * 10);
        else
            clickAnim = MathUtils::animate(1.f, clickAnim, ImRenderUtils::getDeltaTime() * 10);

        // Only render the slider if the background is visible.
        if (bgRect.y > catRect.y + 0.5f)
        {
            // Render the slider background.
            ImRenderUtils::fillRectangle(bgRect, ImColor(30, 30, 30), animation, radius,
                ImGui::GetBackgroundDrawList(), ImDrawFlags_RoundCornersBottom);

            // Calculate the slider knob position based on current value.
            float sliderWidth = sliderArea.z - sliderArea.x;
            float knobPos = (value - minVal) / (maxVal - minVal) * sliderWidth;
            numSetting->sliderEase = MathUtils::animate(knobPos, numSetting->sliderEase, ImRenderUtils::getDeltaTime() * 10);
            numSetting->sliderEase = std::clamp(numSetting->sliderEase, 0.f, sliderArea.z - sliderArea.x);

            // --- Slider Dragging Logic ---
            if (ImRenderUtils::isMouseOver(sliderArea) && isEnabled && isExtended)
            {
                tooltip = numSetting->mDescription;
                if (ImGui::IsMouseDown(0) || ImGui::IsMouseDown(2))
                {
                    numSetting->isDragging = true;
                    lastDraggedSetting = numSetting;
                }
            }
            if (ImGui::IsMouseDown(0) && numSetting->isDragging && isEnabled)
            {
                if (lastDraggedSetting == numSetting)
                {
                    float mouseX = ImRenderUtils::getMousePos().x;
                    float newVal = ((mouseX - sliderArea.x) / (sliderArea.z - sliderArea.x)) * (maxVal - minVal) + minVal;
                    numSetting->setValue(std::clamp(newVal, minVal, maxVal));
                }
                else {
                    numSetting->isDragging = false;
                }
            }
            else if (ImGui::IsMouseDown(2) && numSetting->isDragging && isEnabled)
            {
                if (lastDraggedSetting == numSetting)
                {
                    float mouseX = ImRenderUtils::getMousePos().x;
                    float newVal = ((mouseX - sliderArea.x) / (sliderArea.z - sliderArea.x)) * (maxVal - minVal) + minVal;
                    // Apply midclick rounding for precision.
                    newVal = std::round(newVal / midclickRounding) * midclickRounding;
                    numSetting->mValue = std::clamp(newVal, minVal, maxVal);
                }
                else {
                    numSetting->isDragging = false;
                }
            }
            else {
                numSetting->isDragging = false;
            }
            // --- End Slider Dragging Logic ---

            // Render slider bar.
            float sliderBarHeight = sliderArea.w - sliderArea.y;
            ImVec2 barMin = ImVec2(sliderArea.x, sliderArea.w - sliderBarHeight / 8);
            ImVec2 barMax = ImVec2(sliderArea.x + (numSetting->sliderEase * inScale), sliderArea.w);
            barMin.y = barMax.y - 4 * inScale;
            ImVec4 barRect = ImVec4(barMin.x, barMin.y - 4.5f, barMax.x, barMax.y - 6.5f);
            ImRenderUtils::fillRectangle(barRect, sliderColor, animation, 15);

            // Render slider knob (circle).
            ImVec2 knobCenter = ImVec2(barRect.z - 2.25f, (barRect.y + barRect.w) / 2);
            if (value <= minVal + 0.83f)
                knobCenter.x = barRect.z + 2.25f;
            ImRenderUtils::fillCircle(knobCenter, 5.5f * clickAnim * animation, sliderColor, animation, 12);

            // Render slider shadow.
            ImGui::GetBackgroundDrawList()->PushClipRect(ImVec2(barRect.x, barRect.y),
                ImVec2(barRect.z, barRect.w), true);
            ImRenderUtils::fillShadowRectangle(barRect, sliderColor, animation * 0.75f, 15.f, 0);
            ImGui::GetBackgroundDrawList()->PopClipRect();

            // Draw slider value text and setting name.
            float valueTextWidth = ImRenderUtils::getTextWidth(&valueStr, textSize);
            ImRenderUtils::drawText(ImVec2((bgRect.z - 5.f) - valueTextWidth, bgRect.y + 2.5f),
                valueStr, ImColor(170, 170, 170), textSize, animation, true);
            ImRenderUtils::drawText(ImVec2(bgRect.x + 5.f, bgRect.y + 2.5f),
                setName, ImColor(255, 255, 255), textSize, animation, true);
        }
    }
};
