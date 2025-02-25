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
 * @brief Renders an enum setting UI component.
 *
 * This class encapsulates the logic for drawing an enum setting,
 * including both the base row (with current selection) and expanded options.
 */
class EnumSettingRenderer {
public:
    /**
     * @brief Renders the enum setting.
     *
     * @param enumSetting Pointer to the EnumSetting instance.
     * @param modRect The module rectangle.
     * @param catRect The category rectangle.
     * @param catY The Y coordinate of the current category header.
     * @param catHeight Height of the category header.
     * @param moduleY In/out: current vertical offset for the module row.
     * @param setPadding Vertical padding for the setting row.
     * @param modHeight Height of the module row.
     * @param animation Current animation factor.
     * @param inScale UI scale factor.
     * @param screenHalfY Half the screen height.
     * @param textHeight Height of the text.
     * @param textSize Text size.
     * @param isEnabled Whether the GUI is enabled.
     * @param lowercase Whether to convert names to lowercase.
     * @param moduleCAnim Module animation factor (e.g. mod->cAnim).
     * @param tooltip Out: tooltip text if mouse hovers.
     * @param radius Corner radius for rounded drawing.
     * @param displayColorPicker Whether a color picker is active (to avoid conflict).
     * @param modShowSettings Whether the module’s settings are visible.
     */
    static void render(EnumSetting* enumSetting,
        const ImVec4& modRect,
        const ImVec4& catRect,
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
        bool lowercase,
        float moduleCAnim,
        std::string& tooltip,
        float radius,
        bool displayColorPicker,
        bool modShowSettings)
    {
        // Prepare the enum data.
        std::string setName = lowercase ? StringUtils::toLower(enumSetting->mName) : enumSetting->mName;
        std::vector<std::string> enumValues = enumSetting->mValues;
        if (lowercase) {
            for (auto& val : enumValues)
                val = StringUtils::toLower(val);
        }
        int* currentIndex = &enumSetting->mValue;
        const int numOptions = static_cast<int>(enumValues.size());

        // Animate vertical offset.
        moduleY = MathUtils::lerp(moduleY, moduleY + modHeight, moduleCAnim);

        // Compute the base rectangle for the enum setting row.
        ImVec4 baseRect = ImVec4(
            modRect.x,
            catY + catHeight + moduleY + setPadding,
            modRect.z,
            catY + catHeight + moduleY + modHeight
        ).scaleToPoint(ImVec4(modRect.x, screenHalfY, modRect.z, screenHalfY), inScale);
        baseRect.y = std::floor(baseRect.y);
        if (baseRect.y < modRect.y)
            baseRect.y = modRect.y;

        // Animate the expansion of enum options.
        float targetAnim = (enumSetting->enumExtended && modShowSettings) ? 1.f : 0.f;
        enumSetting->enumSlide = MathUtils::animate(targetAnim, enumSetting->enumSlide, ImRenderUtils::getDeltaTime() * 10);
        enumSetting->enumSlide = MathUtils::clamp(enumSetting->enumSlide, 0.f, 1.f);

        // Render expanded options if applicable.
        if (enumSetting->enumSlide > 0.001f) {
            for (int j = 0; j < numOptions; j++) {
                moduleY = MathUtils::lerp(moduleY, moduleY + modHeight, enumSetting->enumSlide);
                ImVec4 optionRect = ImVec4(
                    modRect.x,
                    catY + catHeight + moduleY + setPadding,
                    modRect.z,
                    catY + catHeight + moduleY + modHeight
                ).scaleToPoint(ImVec4(modRect.x, screenHalfY, modRect.z, screenHalfY), inScale);
                if (optionRect.y > catRect.y + 0.5f) {
                    float optionTextY = optionRect.y + ((optionRect.w - optionRect.y) - textHeight) / 2;
                    // Draw option background.
                    ImRenderUtils::fillRectangle(optionRect, ImColor(20, 20, 20), animation, radius,
                        ImGui::GetBackgroundDrawList(), ImDrawFlags_RoundCornersBottom);
                    // Draw a small indicator if this option is selected.
                    if (*currentIndex == j) {
                        ImRenderUtils::fillRectangle(ImVec4(optionRect.x, optionRect.y, optionRect.x + 1.5f, optionRect.w),
                            ColorUtils::getThemedColor(optionRect.y), animation);
                    }
                    // On mouse click, select this option.
                    if (ImRenderUtils::isMouseOver(optionRect) && ImGui::IsMouseClicked(0) &&
                        isEnabled && !displayColorPicker && modShowSettings)
                    {
                        *currentIndex = j;
                    }
                    // Render the option text.
                    ImRenderUtils::drawText(ImVec2(optionRect.x + 5.f, optionTextY),
                        enumValues[j], ImColor(255, 255, 255), textSize, animation, true);
                }
            }
        }

        // Render the base row for the enum setting.
        if (baseRect.y > catRect.y + 0.5f) {
            ImRenderUtils::fillRectangle(baseRect, ImColor(30, 30, 30), animation, radius,
                ImGui::GetBackgroundDrawList(), ImDrawFlags_RoundCornersBottom);
            // Check for clicks: left-click cycles options; right-click toggles expansion.
            if (ImRenderUtils::isMouseOver(baseRect) && isEnabled) {
                tooltip = enumSetting->mDescription;
                if (ImGui::IsMouseClicked(0) && !displayColorPicker && modShowSettings) {
                    *currentIndex = (*currentIndex + 1) % numOptions;
                }
                else if (ImGui::IsMouseClicked(1) && modShowSettings && !displayColorPicker) {
                    enumSetting->enumExtended = !enumSetting->enumExtended;
                }
            }
            float baseTextY = baseRect.y + ((baseRect.w - baseRect.y) - textHeight) / 2;
            // Draw the setting name on the left.
            ImRenderUtils::drawText(ImVec2(baseRect.x + 5.f, baseTextY),
                setName, ImColor(255, 255, 255), textSize, animation, true);
            // Draw the currently selected option on the right.
            std::string currentOption = enumValues[*currentIndex];
            float optionWidth = ImRenderUtils::getTextWidth(&currentOption, textSize);
            ImRenderUtils::drawText(ImVec2((baseRect.z - 5.f) - optionWidth, baseTextY),
                currentOption, ImColor(170, 170, 170), textSize, animation, true);
        }

        // Render a gradient shadow beneath the enum row.
        if (baseRect.y > catRect.y - modHeight) {
            ImRenderUtils::fillGradientOpaqueRectangle(
                ImVec4(baseRect.x, baseRect.w, baseRect.z, baseRect.w + 10.f * enumSetting->enumSlide * animation),
                ImColor(0, 0, 0), ImColor(0, 0, 0), 0.f, 0.55f * animation);
        }
    }
};