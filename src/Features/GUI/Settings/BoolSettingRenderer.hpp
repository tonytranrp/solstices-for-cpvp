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
 * @brief Renders a boolean (toggle) setting UI component.
 *
 * This class encapsulates the drawing, mouse interaction, and animation for a bool setting.
 */
class BoolSettingRenderer {
public:
    /**
     * @brief Renders a boolean setting.
     *
     * @param boolSetting Pointer to the BoolSetting instance.
     * @param modRect The rectangle representing the module row.
     * @param catY The Y coordinate of the category header.
     * @param catHeight Height of the category header.
     * @param moduleY In/out parameter: current vertical offset for this module row (updated).
     * @param setPadding Additional vertical padding for the setting.
     * @param modHeight Height of one setting row.
     * @param animation Current animation factor.
     * @param inScale UI scale factor.
     * @param screenHalfY Half the screen height (screen.y / 2).
     * @param textHeight Height of the text.
     * @param textSize Size of the text.
     * @param isEnabled Whether the GUI is enabled.
     * @param isExtended Whether the current category is extended.
     * @param tooltip Out parameter: tooltip text if mouse hovers.
     * @param displayColorPicker Whether a color picker is active.
     * @param radius Corner radius for rounded drawing.
     * @param moduleCAnim Module’s animation factor (e.g. mod->cAnim).
     * @param indicatorColor The color used for the checkmark indicator.
     * @param lastBoolSetting In/out: pointer to the last BoolSetting bound.
     * @param isBoolSettingBinding In/out: flag indicating if a bool setting is being bound.
     */
    static void render(BoolSetting* boolSetting,
        const ImVec4& modRect,
        float catY,
        float catHeight,
        float& moduleY,
        float setPadding,
        float modHeight,
        float animation,
        float inScale,
        float screenHalfY,
        float textHeight,
        float textSize,
        bool isEnabled,
        bool isExtended,
        std::string& tooltip,
        bool displayColorPicker,
        float radius,
        float moduleCAnim,
        const ImColor& indicatorColor,
        BoolSetting*& lastBoolSetting,
        bool& isBoolSettingBinding)
    {
        // Determine display name (assumes boolSetting->mName is a std::string)
        std::string setName = boolSetting->mName;

        // Animate the vertical offset for this setting row.
        moduleY = MathUtils::lerp(moduleY, moduleY + modHeight, moduleCAnim);

        // Calculate the base rectangle for the bool setting row.
        ImVec4 rect = ImVec4(
            modRect.x,
            catY + catHeight + moduleY + setPadding,
            modRect.z,
            catY + catHeight + moduleY + modHeight
        ).scaleToPoint(ImVec4(modRect.x, screenHalfY, modRect.z, screenHalfY), inScale);
        rect.y = std::floor(rect.y);
        if (rect.y < modRect.y)
            rect.y = modRect.y; // Ensure the rectangle doesn't start above the module

        // Render only if the rectangle is sufficiently low (visible).
        if (rect.y > (catY + catHeight) + 0.5f)
        {
            // Draw the background for the bool setting.
            ImRenderUtils::fillRectangle(rect, ImColor(30, 30, 30), animation, radius,
                ImGui::GetBackgroundDrawList(), ImDrawFlags_RoundCornersBottom);

            // Handle interaction: if mouse hovers over the rect and the GUI is enabled.
            if (ImRenderUtils::isMouseOver(rect) && isEnabled && isExtended)
            {
                tooltip = boolSetting->mDescription;
                // Toggle the boolean value on left-click.
                if (ImGui::IsMouseClicked(0) && !displayColorPicker /* && mod->showSettings assumed true */) {
                    boolSetting->mValue = !boolSetting->mValue;
                }
                // Enable binding mode on middle-click.
                if (ImGui::IsMouseClicked(2) && !displayColorPicker && isExtended) {
                    lastBoolSetting = boolSetting;
                    isBoolSettingBinding = true;
                    ClientInstance::get()->playUi("random.pop", 0.75f, 1.0f);
                }
            }

            // Animate the bool indicator (boolScale) toward 1 if true, or 0 if false.
            boolSetting->boolScale = MathUtils::animate(boolSetting->mValue ? 1.f : 0.f,
                boolSetting->boolScale,
                ImRenderUtils::getDeltaTime() * 10);

            // Calculate vertical centering for the setting text.
            float textY = rect.y + ((rect.w - rect.y) - textHeight) / 2;

            // Prepare a "shadow circle" for the checkmark indicator.
            ImVec4 smoothRect = ImVec4(rect.z - 19, rect.y + 5, rect.z - 5, rect.w - 5);
            ImVec2 circleCenter = ImVec2(smoothRect.x + smoothRect.getWidth() / 2.f,
                smoothRect.y + smoothRect.getHeight() / 2.f);
            ImColor baseColor(15, 15, 15);
            // Lerp from baseColor to the provided indicatorColor based on boolScale.
            ImColor shadowCol = MathUtils::lerpImColor(baseColor, indicatorColor, boolSetting->boolScale);
            ImRenderUtils::fillShadowCircle(circleCenter, 5, shadowCol, animation * moduleCAnim, 40, 0);

            // Draw the checkmark indicator.
            ImVec4 indicatorRect = ImVec4(rect.z - 23.5f, textY - 2.5f, rect.z - 5, textY + 17.5f)
                .scaleToPoint(ImVec4(rect.z, rect.y, rect.z, rect.w), animation);
            float indicatorWidth = indicatorRect.z - indicatorRect.x;
            if (boolSetting->boolScale > 0.01f)
            {
                // Ensure the indicator doesn't extend above the module rectangle.
                if (indicatorRect.y < modRect.w)
                    indicatorRect.y = modRect.w;
                auto fgList = ImGui::GetForegroundDrawList();
                fgList->PushClipRect(ImVec2(indicatorRect.x, indicatorRect.y),
                    ImVec2(indicatorRect.x + indicatorWidth * boolSetting->boolScale, indicatorRect.w), true);
                // Draw the checkmark twice for emphasis.
                ImRenderUtils::drawCheckMark(ImVec2(indicatorRect.getCenter().x - (4 * animation),
                    indicatorRect.getCenter().y - (1 * animation)),
                    1.3 * animation, indicatorColor, moduleCAnim * animation);
                ImRenderUtils::drawCheckMark(ImVec2(indicatorRect.getCenter().x - (4 * animation),
                    indicatorRect.getCenter().y - (1 * animation)),
                    1.3 * animation, indicatorColor, moduleCAnim * animation);
                fgList->PopClipRect();
            }

            // Render the setting name on the left.
            ImRenderUtils::drawText(ImVec2(rect.x + 5.f, textY),
                setName, ImColor(255, 255, 255), textSize, animation, true);
        }
    }
};