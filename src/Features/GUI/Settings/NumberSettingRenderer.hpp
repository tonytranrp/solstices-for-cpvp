#pragma once
#include <cmath>
#include <algorithm>
#include <string>
#include <sstream>
#include <iomanip>
#include <Features/FeatureManager.hpp>
#include <Features/Modules/Setting.hpp>
#include <Features/Modules/ModuleCategory.hpp>
#include <Features/Modules/Visual/ClickGui.hpp>
#include <Utils/FontHelper.hpp>
#include <Utils/MiscUtils/ImRenderUtils.hpp>
#include <Utils/MiscUtils/MathUtils.hpp>
#include <Features/Modules/Visual/Interface.hpp>
#include <SDK/Minecraft/ClientInstance.hpp>
#include <SDK/Minecraft/Rendering/GuiData.hpp>
#include <Utils/Keyboard.hpp>
#include <Utils/StringUtils.hpp>
#include <Utils/MiscUtils/ColorUtils.hpp>

/**
 * @brief Renders a floating-point slider (NumberSetting) in a clickable UI row.
 *
 * Changes & Highlights:
 * 1) Replaced sprintf_s with a small i/o stream approach for controlling precision.
 * 2) Merged repetitive logic for reading the mouse X and clamping the new slider value.
 * 3) Reduced nesting: clearer checks for left-click vs. middle-click dragging.
 * 4) Comments focused on the big changes, while smaller/obvious parts are left minimal.
 */
class NumberSettingRenderer {
public:
    static void render(NumberSetting* setting,
        const ImVec4& moduleRect,
        const ImVec4& categoryRect,
        float          baseY,
        float          rowHeight,
        float          rowPadding,
        float          animation,
        float          uiScale,
        float          screenHalfY,
        float          textHeight,
        float          fontSize,
        bool           guiEnabled,
        bool           lowercase,
        float          moduleAnimFactor,
        float& currentY,
        float          midclickRounding,
        bool           showSettings,
        bool           categoryExtended,
        std::string& outTooltip,
        float          cornerRadius,
        Setting*& lastDraggedSetting)
    {
        if (!setting) return;

        // (1) Current value and slider bounds
        float curValue = setting->mValue;
        float minVal = setting->mMin;
        float maxVal = setting->mMax;

        // (2) Convert the current value to a string with two decimals
        std::ostringstream valueStream;
        valueStream << std::fixed << std::setprecision(2) << curValue;
        std::string valueStr = valueStream.str();

        // (3) Possibly lowercase the displayed name
        std::string displayName = lowercase ? StringUtils::toLower(setting->mName) : setting->mName;

        // (4) Slide the vertical offset (show/hide animations)
        currentY = MathUtils::lerp(currentY, currentY + rowHeight, moduleAnimFactor);

        // (5) Main background rectangle for this row
        ImVec4 bgRect(
            moduleRect.x,
            baseY + currentY,
            moduleRect.z,
            baseY + currentY + rowHeight
        );
        bgRect = bgRect.scaleToPoint(
            ImVec4(moduleRect.x, screenHalfY, moduleRect.z, screenHalfY), uiScale
        );
        bgRect.y = std::max(std::floor(bgRect.y), moduleRect.y);

        // (6) Slider area with horizontal padding
        ImVec4 sliderRect(
            moduleRect.x + 7.f,
            baseY + currentY + rowPadding,
            moduleRect.z - 7.f,
            baseY + currentY + rowHeight
        );
        sliderRect = sliderRect.scaleToPoint(
            ImVec4(moduleRect.x, screenHalfY, moduleRect.z, screenHalfY), uiScale
        );
        sliderRect.y = std::max(std::floor(sliderRect.y), moduleRect.y);

        // (7) Color for the slider bar/knob
        ImColor sliderColor = ColorUtils::getThemedColor(currentY * 2); // example

        // (8) Simple click animation factor (shrinks knob slightly while pressed)
        static float clickAnim = 1.f;
        bool hoveringSlider = ImRenderUtils::isMouseOver(sliderRect);
        bool pressingLeft = ImGui::IsMouseDown(0);
        if (pressingLeft && hoveringSlider)
            clickAnim = MathUtils::animate(0.6f, clickAnim, ImRenderUtils::getDeltaTime() * 10);
        else
            clickAnim = MathUtils::animate(1.f, clickAnim, ImRenderUtils::getDeltaTime() * 10);

        // (9) Only draw if row is visible below category header
        if (bgRect.y > categoryRect.y + 0.5f)
        {
            // Background fill
            ImRenderUtils::fillRectangle(
                bgRect, ImColor(30, 30, 30),
                animation, cornerRadius,
                ImGui::GetBackgroundDrawList(),
                ImDrawFlags_RoundCornersBottom
            );

            // --- Slider Position Calculation ---
            float range = maxVal - minVal;
            float sliderSize = (sliderRect.z - sliderRect.x);
            float targetPos = (curValue - minVal) / range * sliderSize;

            // Smoothly animate “sliderEase” toward new target
            setting->sliderEase = MathUtils::animate(
                targetPos,
                setting->sliderEase,
                ImRenderUtils::getDeltaTime() * 10
            );
            setting->sliderEase = std::clamp(setting->sliderEase, 0.f, sliderSize);

            // --- Handle Interaction & Dragging ---
            if (hoveringSlider && guiEnabled && categoryExtended)
            {
                // Show tooltip on hover
                outTooltip = setting->mDescription;

                // Start dragging if left/middle mouse is down
                if (ImGui::IsMouseDown(0) || ImGui::IsMouseDown(2))
                {
                    setting->isDragging = true;
                    lastDraggedSetting = setting;
                }
            }

            // If we’re actually dragging
            if (setting->isDragging && guiEnabled)
            {
                float mouseX = ImRenderUtils::getMousePos().x;
                float relative = (mouseX - sliderRect.x) / (sliderRect.z - sliderRect.x);
                float newVal = minVal + std::clamp(relative, 0.f, 1.f) * range;

                // Left-click => normal drag
                if (ImGui::IsMouseDown(0) && lastDraggedSetting == setting) {
                    setting->setValue(newVal);
                }
                // Middle-click => precise drag with rounding
                else if (ImGui::IsMouseDown(2) && lastDraggedSetting == setting) {
                    newVal = std::round(newVal / midclickRounding) * midclickRounding;
                    setting->mValue = std::clamp(newVal, minVal, maxVal);
                }
                // If no relevant mouse button, stop dragging
                else {
                    setting->isDragging = false;
                }
            }

            // --- Render the “filled bar” and knob ---
            float barHeight = sliderRect.w - sliderRect.y;
            ImVec2 barMin = ImVec2(sliderRect.x, sliderRect.w - barHeight / 8.f);
            ImVec2 barMax = ImVec2(sliderRect.x + (setting->sliderEase * uiScale), sliderRect.w);
            barMin.y = barMax.y - 4 * uiScale;

            // Bar rect for fillRectangle
            ImVec4 barRect(barMin.x, barMin.y - 4.5f, barMax.x, barMax.y - 6.5f);
            ImRenderUtils::fillRectangle(barRect, sliderColor, animation, 15);

            // Knob (small circle on the bar)
            float knobRadius = 5.5f * clickAnim * animation;
            float knobX = barRect.z - 2.25f;
            float knobY = (barRect.y + barRect.w) * 0.5f;
            if (curValue <= (minVal + 0.83f)) // small offset fix
                knobX = barRect.z + 2.25f;
            ImRenderUtils::fillCircle(ImVec2(knobX, knobY), knobRadius, sliderColor, animation, 12);

            // Optional shadow effect behind the bar
            ImGui::GetBackgroundDrawList()->PushClipRect(
                ImVec2(barRect.x, barRect.y),
                ImVec2(barRect.z, barRect.w),
                true
            );
            ImRenderUtils::fillShadowRectangle(
                barRect, sliderColor,
                animation * 0.75f, 15.f, 0
            );
            ImGui::GetBackgroundDrawList()->PopClipRect();

            // --- Text: value on the right, setting name on the left ---
            float valTextW = ImRenderUtils::getTextWidth(&valueStr, fontSize);

            ImRenderUtils::drawText(
                ImVec2((bgRect.z - 5.f) - valTextW, bgRect.y + 2.5f),
                valueStr,
                ImColor(170, 170, 170),
                fontSize,
                animation,
                true
            );

            ImRenderUtils::drawText(
                ImVec2(bgRect.x + 5.f, bgRect.y + 2.5f),
                displayName,
                ImColor(255, 255, 255),
                fontSize,
                animation,
                true
            );
        }
    }
};
